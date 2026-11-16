FROM gcc:15-bookworm
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build pkg-config libsdl2-dev libsdl2-mixer-dev libgl1-mesa-dev libegl1-mesa-dev libcurl4-openssl-dev libjansson-dev libmbedtls-dev xvfb xauth python3 zbar-tools && rm -rf /var/lib/apt/lists/*
ENV SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1
WORKDIR /app
CMD ["bash", "tests/run-ui.sh"]
