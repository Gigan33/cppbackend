FROM gcc:11.3

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

COPY ./src /app/src
COPY ./data /app/data
COPY CMakeLists.txt conanfile.txt /app/

RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

EXPOSE 8080

CMD ["./build/bin/game_server", "./data/config.json"]
