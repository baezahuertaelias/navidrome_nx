FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends g++ libcurl4-openssl-dev libjansson-dev libmbedtls-dev python3 && rm -rf /var/lib/apt/lists/*
WORKDIR /app
CMD ["bash", "-c", "g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/api_test.cpp -lcurl -ljansson -lmbedcrypto -o /tmp/api_test && python3 tests/run.py && g++ -std=c++17 -Isrc -isystem vendor/borealis/library/lib/extern/nanovg-deko3d/include tests/artwork_test.cpp -o /tmp/artwork_test && /tmp/artwork_test"]
