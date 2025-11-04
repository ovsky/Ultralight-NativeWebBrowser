#!/usr/bin/env sh
set -e

# Wrapper to launch the installed app from its installed directory.
# This keeps .desktop Exec path stable and allows us to ship a self-contained tree under ${INSTALL_DIR_NAME}.

PREFIX=${PREFIX:-/usr/local}
APP_DIR="${PREFIX}/UltralightWebBrowser"
APP_BIN="${APP_DIR}/Ultralight-WebBrowser"

# If packaged to /usr instead of /usr/local, fall back to /usr
if [ ! -x "$APP_BIN" ] && [ -x "/usr/UltralightWebBrowser/Ultralight-WebBrowser" ]; then
  APP_DIR="/usr/UltralightWebBrowser"
  APP_BIN="${APP_DIR}/Ultralight-WebBrowser"
fi

exec "$APP_BIN" "$@"