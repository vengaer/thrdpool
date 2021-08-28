FROM archlinux:latest
LABEL maintainer="vilhelm.engstrom@tuta.io"

RUN pacman -Syu --needed --noconfirm make clang gcc git ruby cmake

ENV CC=gcc

COPY . /thrdpool
WORKDIR /thrdpool
