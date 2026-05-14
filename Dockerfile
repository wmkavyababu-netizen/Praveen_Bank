FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libssl-dev \
    libasio-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Crow (header-only web framework)
RUN git clone --branch v1.0+5 --depth 1 https://github.com/CrowCpp/Crow.git /tmp/Crow && \
    cp /tmp/Crow/include/crow.h /usr/local/include/ && \
    cp -r /tmp/Crow/include/crow /usr/local/include/ && \
    rm -rf /tmp/Crow

# Set working directory
WORKDIR /app

# Copy the project
COPY . .

# Build project
RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    cmake --build . --parallel $(nproc)

# Expose app port
EXPOSE 18080

# Start backend server
CMD ["./build/praveen_bank"]