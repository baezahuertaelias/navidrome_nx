FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends g++ libcurl4-openssl-dev libjansson-dev libmbedtls-dev python3 zbar-tools && rm -rf /var/lib/apt/lists/*
WORKDIR /app
CMD ["bash", "tests/run.sh"]
