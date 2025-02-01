FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    qemu \
    qemu-user-static \
	gcc-aarch64-linux-gnu \
    binfmt-support \
    qemu-utils \
    debootstrap \
    g++ \
    build-essential \
    cmake \
    git \
    wget \
    curl \
    vim \
    sudo \
	flex \
	bison \
	libelf-dev \
	build-essential \
	bc \
	ninja-build \
	pkg-config \
	zlib1g-dev \
	meson \
	python3 \
	python3-pip \
	libglib2.0-dev \
	libpixman-1-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

CMD ["/bin/bash"]
