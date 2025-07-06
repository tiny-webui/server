# Exit if not being sourced
(return 0 2>/dev/null)
if [[ $? -ne 0 ]]; then
    echo "This script must be sourced, not executed."
    echo "Use: source $0"
    exit 1
fi

if [[ -n "$TUI_EXTRA_INCLUDE_PATH" ]]; then
    echo "Env already set, skipping."
    return 0
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
LOCAL_LIB_PATH_FILE="${SCRIPT_DIR}/local-lib-path"
if [[ ! -f "$LOCAL_LIB_PATH_FILE" ]]; then
    echo "Error: $LOCAL_LIB_PATH_FILE not found."
    return 1
fi

LOCAL_LIB_PATH=$(cat "$LOCAL_LIB_PATH_FILE")
if [[ ! -d "$LOCAL_LIB_PATH" ]]; then
    echo "Error: Path '$LOCAL_LIB_PATH' does not exist."
    return 1
fi

export TUI_EXTRA_INCLUDE_PATH="$LOCAL_LIB_PATH/include"
export TUI_EXTRA_LIB_PATH="$LOCAL_LIB_PATH/lib"
export LD_LIBRARY_PATH="$LOCAL_LIB_PATH/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$LOCAL_LIB_PATH/lib/pkgconfig:$PKG_CONFIG_PATH"

