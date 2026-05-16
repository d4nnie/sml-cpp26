FROM gcc:16

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        python3 \
        python3-venv \
        python3-pip \
        git \
        ca-certificates \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Conan 2.x in an isolated venv (PEP 668 makes system pip uncomfortable).
RUN python3 -m venv /opt/venv \
    && /opt/venv/bin/pip install --no-cache-dir 'conan>=2.7,<3'
ENV PATH="/opt/venv/bin:${PATH}"

# Conan profile that wires up gcc-16 + C++26 + reflection flag, and a
# settings_user.yml that teaches Conan about gcc-16 (built-in settings only
# enumerate up to gcc-15 in current Conan 2.x).
RUN conan profile detect --force >/dev/null
COPY conan-settings-user.yml /root/.conan2/settings_user.yml
COPY conan-profile-gcc16 /root/.conan2/profiles/gcc16

WORKDIR /work
