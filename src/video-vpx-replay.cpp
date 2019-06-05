/*
 * Copyright (C) 2019  Christian Berger
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#include <libyuv.h>
#include <X11/Xlib.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{1};
    const std::string PROGRAM{argv[0]}; // NOLINT
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("cid")) ||
         (0 == commandlineArguments.count("name")) ) {
        std::cerr << argv[0] << " replays Envelopes from a given .rec file while restoring contained VP8 or VP9 frames as ARGB frames into a shared memory area." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --name=<name of shared memory area> [--id=<ID>] [--gpu=<number of GPU to use>] [--verbose] file.rec" << std::endl;
        std::cerr << "         --cid:     CID of the OD4Session to replay other Envelopes" << std::endl;
        std::cerr << "         --id:      when using several instances, only decode h264 with this senderStamp; default: use the first ImageReading" << std::endl;
        std::cerr << "         --name:    name of the shared memory area to create" << std::endl;
        std::cerr << "         --verbose: print decoding information and display image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=111 --name=data --verbose" << std::endl;
    }
    else {
        const std::string NAME{commandlineArguments["name"]};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        bool isIDset{(commandlineArguments["id"].size() != 0)};
        uint32_t ID{isIDset ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};

        std::string recFile;
        for (auto e : commandlineArguments) {
            if (e.second.empty() && e.first != PROGRAM) {
                recFile = e.first;
                break;
            }
        }

        std::fstream fin(recFile, std::ios::in|std::ios::binary);
        if (!recFile.empty() && fin.good()) {
            fin.close();

//            std::unique_ptr<NvDecoder> decoder{nullptr};

            // Interface to a running OpenDaVINCI session (ignoring any incoming Envelopes).
            cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

            std::unique_ptr<cluon::SharedMemory> sharedMemory(nullptr);

            Display *display{nullptr};
            Visual *visual{nullptr};
            Window window{0};
            XImage *ximage{nullptr};

            auto onNewImage = [&isIDset, &sharedMemory, &display, &visual, &window, &ximage, &NAME, &VERBOSE, &ID](cluon::data::Envelope &&env){
                if (!isIDset) {
                    isIDset = true;
                    ID = env.senderStamp();
                    std::clog << "[video-vpx-replay]: Using senderStamp " << ID << std::endl;
                }
                if (ID == env.senderStamp()) {
                    cluon::data::TimeStamp sampleTimeStamp = env.sampleTimeStamp();
                    opendlv::proxy::ImageReading img = cluon::extractMessage<opendlv::proxy::ImageReading>(std::move(env));
                    if ("h264" == img.fourcc()) {
                        const uint32_t WIDTH = img.width();
                        const uint32_t HEIGHT = img.height();

                        if (!sharedMemory) {
                            std::clog << "[video-vpx-replay]: Created shared memory " << NAME << " (" << (WIDTH * HEIGHT * 4) << " bytes) for an ARGB image (width = " << WIDTH << ", height = " << HEIGHT << ")." << std::endl;
                            sharedMemory.reset(new cluon::SharedMemory{NAME, WIDTH * HEIGHT * 4});
                            if (VERBOSE) {
                                display = XOpenDisplay(NULL);
                                visual = DefaultVisual(display, 0);
                                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                                ximage = XCreateImage(display, visual, 24, ZPixmap, 0, reinterpret_cast<char*>(sharedMemory->data()), WIDTH, HEIGHT, 32, 0);
                                XMapWindow(display, window);
                            }
                        }

                        bool printVideoInformation{false};
//                        if (nullptr == decoder) {
//                            printVideoInformation = true;
//                            decoder.reset(new NvDecoder(cuContext, WIDTH, HEIGHT, false, cudaVideoCodec_H264));
//                        }

                        if (sharedMemory /* && decoder*/) {
                            std::string data{img.data()};
                            const uint32_t LEN{static_cast<uint32_t>(data.size())};

                            if (printVideoInformation) {
//                                std::cerr << "[video-vpx-replay]: " << std::endl << decoder->GetVideoInfo() << std::endl;
                            }

//                            if (0 == nFrameReturned) {
//                                std::cerr << "h264 decoding for current frame failed." << std::endl;
//                            }
//                            else {
                                sharedMemory->lock();
                                sharedMemory->setTimeStamp(sampleTimeStamp);
                                {
////                                        Nv12ToBgra32(static_cast<uint8_t*>(ppFrame[i]), decoder->GetWidth(), reinterpret_cast<uint8_t*>(sharedMemory->data()), WIDTH * 4, WIDTH, HEIGHT);
//                                        libyuv::NV12ToARGB(static_cast<uint8_t*>(ppFrame[i]), WIDTH, static_cast<uint8_t*>(ppFrame[i])+(WIDTH*HEIGHT), WIDTH, reinterpret_cast<uint8_t*>(sharedMemory->data()), WIDTH * 4, WIDTH, HEIGHT);

                                        if (VERBOSE) {
                                            XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                                        }
                                }
                                sharedMemory->unlock();
                                sharedMemory->notifyAll();
//                            }
                        }
                    }
                }
            };

            constexpr bool AUTOREWIND{false};
            constexpr bool THREADING{true};
            cluon::Player player(recFile, AUTOREWIND, THREADING);

            while (player.hasMoreData() && od4.isRunning()) {
                auto next = player.getNextEnvelopeToBeReplayed();
                if (next.first) {
                    cluon::data::Envelope e = next.second;

                    int64_t decodingTime{0};
                    if (e.dataType() == opendlv::proxy::ImageReading::ID()) {
                        cluon::data::TimeStamp before = cluon::time::now();
                        onNewImage(std::move(e));
                        cluon::data::TimeStamp after = cluon::time::now();

                        decodingTime = cluon::time::deltaInMicroseconds(after, before);
                        decodingTime = (decodingTime < 0) ? 0 : decodingTime;
                    }
                    else {
                        od4.send(std::move(e));
                    }

                    int64_t sleepingTime = static_cast<int64_t>(player.delay()) - decodingTime;
                    sleepingTime = (sleepingTime < 0) ? 0 : sleepingTime;
                    if (0 < sleepingTime) {
                        std::this_thread::sleep_for(std::chrono::duration<int32_t, std::micro>(sleepingTime));
                    }
                }
            }

            if (VERBOSE && display) {
                XCloseDisplay(display);
            }
            retCode = 0;
        }
        else {
            std::cerr << argv[0] << ": Failed to open '" << recFile << "'" << std::endl;
        }
    }
    return retCode;
}

