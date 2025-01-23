#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <fstream>

#include <asio.hpp>

#include <aws/core/Aws.h>
#include <aws/core/utils/threading/Semaphore.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/transcribestreaming/TranscribeStreamingServiceClient.h>
#include <aws/transcribestreaming/model/StartStreamTranscriptionHandler.h>
#include <aws/transcribestreaming/model/StartStreamTranscriptionRequest.h>
#include <aws/transcribestreaming/model/Sentiment.h>
#include <aws/core/platform/FileSystem.h>

#include "AsioDeps.h"

#define AUDIO_FILE_NAME_CH0 "growing_potatoes2.raw"
#define AUDIO_FILE_NAME_CH1 "growing_onions.raw"
#define CHUNK_LENGTH_MS 100

using namespace Aws::TranscribeStreamingService;
using namespace Aws::TranscribeStreamingService::Model;

struct SessionContext
{
    std::unique_ptr<std::ifstream> audioFileCh0;
    std::unique_ptr<std::ifstream> audioFileCh1;
    AudioStream* audioStream = nullptr;
    std::unique_ptr<asio::steady_timer> timer;
};

void sendNextFrame(const std::error_code &ec, const std::shared_ptr<SessionContext>& sessionContext)
{
    const size_t BYTES_PER_SAMPLE = 2;
    const size_t NUM_CHANNELS = 2;
    const size_t SAMPLES_PER_SECOND = 8000;
    const size_t NUM_BYTES_IN_100MS_STEREO_AUDIO = BYTES_PER_SAMPLE * NUM_CHANNELS * (SAMPLES_PER_SECOND * CHUNK_LENGTH_MS / 1000);
    Aws::Vector<unsigned char> eventBuff(NUM_BYTES_IN_100MS_STEREO_AUDIO);
    
    for (size_t bc = 0; bc < NUM_BYTES_IN_100MS_STEREO_AUDIO; bc+=(BYTES_PER_SAMPLE*NUM_CHANNELS))
    {
        sessionContext->audioFileCh0->read((char*)&eventBuff[bc], 2);
        sessionContext->audioFileCh1->read((char*)&eventBuff[bc+BYTES_PER_SAMPLE], 2);
    }

    AudioEvent audioEvent(std::move(eventBuff));
    if (!sessionContext->audioStream->WriteAudioEvent(audioEvent))
    {
        std::cout << "Failed to write an audio event" << std::endl;
        sessionContext->audioStream->Close();
        return;
    }

    sessionContext->timer->expires_after(std::chrono::milliseconds(CHUNK_LENGTH_MS - 10));
    sessionContext->timer->async_wait(std::bind(&sendNextFrame, asio::placeholders::error, sessionContext));
}

int main(int, char**){
    std::cout << "Hello, from ace-media-service!" << std::endl;
    AsioDeps asio;
    asio.initialize();

    std::shared_ptr<SessionContext> sessionContext = std::make_shared<SessionContext>();
    sessionContext->audioFileCh0 = std::make_unique<std::ifstream>(AUDIO_FILE_NAME_CH0, std::ios_base::binary);
    sessionContext->audioFileCh1 = std::make_unique<std::ifstream>(AUDIO_FILE_NAME_CH1, std::ios_base::binary);
    sessionContext->timer = std::make_unique<asio::steady_timer>(*asio.io_service());

    if (sessionContext->audioFileCh0->fail())
    {
        std::cout << "ERROR: Unable to open file " << AUDIO_FILE_NAME_CH0 << std::endl;
        return -1;
    }
    if (sessionContext->audioFileCh1->fail())
    {
        std::cout << "ERROR: Unable to open file " << AUDIO_FILE_NAME_CH1 << std::endl;
        return -1;
    }

    Aws::SDKOptions options;
    Aws::InitAPI(options);
    const Aws::String region = Aws::Region::US_EAST_2;
    Aws::Client::ClientConfiguration config;
    config.region = region;

    std::shared_ptr<Aws::TranscribeStreamingService::TranscribeStreamingServiceClient> transcribeStreamingClient = std::make_shared<TranscribeStreamingServiceClient>(config);
    StartStreamTranscriptionHandler transcriptionHandler;
    transcriptionHandler.SetOnErrorCallback([&sessionContext](const Aws::Client::AWSError<TranscribeStreamingServiceErrors> &error)
    {
        std::cout << "ERROR: " + error.GetMessage() << std::endl;
    });
    transcriptionHandler.SetTranscriptEventCallback([](const TranscriptEvent &ev)
    {
        if (ev.GetTranscript().ResultsHasBeenSet()) {
            const auto& results = ev.GetTranscript().GetResults();
            for (const auto& r : results) {
                if (r.GetAlternatives().size() > 0 && ! r.GetIsPartial()) {
                    std::cout << "transcript chunk: " << r.GetChannelId() << ": " << r.GetAlternatives()[0].GetTranscript() << std::endl;
                    break;
                }
            }
        }
    });
    transcriptionHandler.SetInitialResponseCallbackEx([](const StartStreamTranscriptionInitialResponse &resp, const Aws::Utils::Event::InitialResponseType respType)
    {
        if (!resp.GetSessionId().empty())
        {
            std::cout << "new transcribe session: " << resp.GetSessionId() << std::endl;
        } 
    });
    std::unique_ptr<StartStreamTranscriptionRequest> transcribeRequest = std::make_unique<StartStreamTranscriptionRequest>();
    transcribeRequest->SetMediaSampleRateHertz(8000);
    transcribeRequest->SetLanguageCode(LanguageCode::en_US);
    transcribeRequest->SetMediaEncoding(MediaEncoding::pcm);
    transcribeRequest->SetEventStreamHandler(transcriptionHandler);
    transcribeRequest->SetEnablePartialResultsStabilization(true);
    transcribeRequest->SetPartialResultsStability(PartialResultsStability::high);
    transcribeRequest->SetNumberOfChannels(2);
    transcribeRequest->SetEnableChannelIdentification(true);

    StartStreamTranscriptionStreamReadyHandler streamReadyHandler = [&asio, &sessionContext](AudioStream &stream)
    {
        std::cout << "stream ready" << std::endl;
        sessionContext->audioStream = &stream;

        std::error_code ec;
        asio.io_service()->post(std::bind(&sendNextFrame, ec, sessionContext));
    };

    StartStreamTranscriptionResponseReceivedHandler responseReceivedHandler = [&sessionContext](
                                                     const TranscribeStreamingServiceClient * /*unused*/,
                                                     const Model::StartStreamTranscriptionRequest & /*unused*/,
                                                     const Model::StartStreamTranscriptionOutcome &outcome,
                                                     const std::shared_ptr<const Aws::Client::AsyncCallerContext> & /*unused*/)
    {
        std::cout << "response received" << std::endl;

        if (!outcome.IsSuccess())
        {
            std::cout << "Transcribe streaming error "
                        << outcome.GetError().GetMessage() << std::endl;
        }
    };

    transcribeStreamingClient->StartStreamTranscriptionAsync(*transcribeRequest, streamReadyHandler, responseReceivedHandler);

    std::string answer;
    std::cerr << "Please press <return> key to exit ..." << std::flush;
    std::getline(std::cin, answer);

    std::cout << "exiting" << std::endl;
    Aws::ShutdownAPI(options);
    asio.shutdown();
}
