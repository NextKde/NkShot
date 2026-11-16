#!/bin/sh
# stdout is exclusively JSONL. Never install dependencies or invoke sudo.
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' '{"schemaVersion":1,"type":"result","appId":"org.nextkde.nkshot","action":"unknown","success":false,"code":"DEPENDENCY_UNSATISFIED","stage":"bootstrap","message":"Python 3.9 or newer is required.","details":{"dependencies":[{"id":"python3","required":">=3.9","installed":null,"reason":"missing"}]}}'
    exit 11
fi
exec python3 "$(dirname "$0")/packaging/installer.py" "$@"
