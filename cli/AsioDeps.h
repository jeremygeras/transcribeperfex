#pragma once

#include <memory>
#include <vector>
#include <thread>
#include <iostream>

#include <asio.hpp>

class AsioDeps
{
public:
    AsioDeps() {
        mIoService = std::make_shared<asio::io_context>();
        mWork = std::make_unique<asio::io_service::work>(*mIoService);
    }
    virtual ~AsioDeps() {}

    int initialize() {
        std::size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
            numThreads = 2;
        }
        for (int i=0; i<numThreads; i++) {
            mThreads.emplace_back([this]() {
                this->mIoService->run();
            });
        }
        return 0;
    }

    int shutdown() {
        mIoService->stop();
        for (auto& t : mThreads) {
            t.join();
        }
        std::cout << "IoServiceHolder threads have exited" << std::endl;
        return 0;
    }

    const std::shared_ptr<asio::io_service>& io_service() const {
        return mIoService;
    }

private:
    std::shared_ptr<asio::io_service> mIoService;
    std::unique_ptr<asio::io_service::work> mWork;
    std::vector<std::thread> mThreads;
};
