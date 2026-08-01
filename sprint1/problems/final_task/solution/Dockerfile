FROM gcc:11.3

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

COPY . /app

WORKDIR /app

RUN mkdir -p build && cd build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make

EXPOSE 8080

CMD ["./build/bin/game_server", "./data/config.json"]
