FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y build-essential git gdb cmake bsdmainutils libboost-system-dev
RUN git clone https://github.com/Tencent/rapidjson.git && \
    cp -r rapidjson/include/rapidjson /usr/local/include/
RUN git clone https://github.com/uNetworking/uWebSockets.git && \
    cd uWebSockets && \
    git submodule init && \
    git submodule update uSockets && \
    git checkout c2f29c86c718e364b5586d103ad592aa396d7604 && \
    make install && \
    cd uSockets && \
    make && \
    cp src/libusockets.h /usr/local/include/ && \
    cp uSockets.a /usr/local/lib
RUN git clone https://github.com/Forestryks/libsbox.git && \
    mkdir libsbox/build && \
    cd libsbox/build && \
    cmake .. && \
    make && \
    yes | make install

COPY . /polygraph
WORKDIR /polygraph

RUN rm -rf cmake-build-asan && \
    mkdir cmake-build-asan && \
    cd cmake-build-asan && \
    cmake .. \
    -D CMAKE_BUILD_TYPE=ASan \
    -D ENABLE_RUNNER=1 \
    -D ENABLE_SCHEDULER=1 \
    -D ENABLE_TESTING=1 && \
    make

ENTRYPOINT [ "./entrypoint.sh" ]