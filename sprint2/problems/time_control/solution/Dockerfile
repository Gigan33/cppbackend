# Первый контейнер — сборка
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# Устанавливаем зависимости через Conan
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

# Копируем исходники и собираем
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер — запуск
FROM ubuntu:22.04 as run

# Создаём пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Копируем приложение и данные
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

# Запускаем сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json", "/app/static/"]
