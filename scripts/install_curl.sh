#!/usr/bin/env bash
set -euo pipefail

echo "Detecting package manager and installing libcurl development package..."
if command -v apt-get >/dev/null 2>&1; then
  echo "APT detected. Installing libcurl4-openssl-dev (Debian/Ubuntu)..."
  sudo apt-get update
  sudo apt-get install -y libcurl4-openssl-dev
  exit 0
fi
if command -v dnf >/dev/null 2>&1; then
  echo "DNF detected. Installing libcurl-devel (Fedora/RHEL)..."
  sudo dnf install -y libcurl-devel
  exit 0
fi
if command -v pacman >/dev/null 2>&1; then
  echo "pacman detected. Installing libcurl (Arch Linux)..."
  sudo pacman -Sy --noconfirm curl
  exit 0
fi
if command -v brew >/dev/null 2>&1; then
  echo "Homebrew detected. Installing curl..."
  brew install curl
  exit 0
fi

echo "No known package manager detected; please install libcurl (development headers) manually."
exit 1
