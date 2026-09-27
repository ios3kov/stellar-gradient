#!/usr/bin/env python3
import pathlib
import re
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: verify_shell_metadata.py BUILD_RS SHELL_CPP")

build = pathlib.Path(sys.argv[1]).read_text()
shell = pathlib.Path(sys.argv[2]).read_text()

def required(pattern: str, text: str, label: str) -> str:
    match = re.search(pattern, text, re.MULTILINE)
    if not match:
        raise SystemExit(f"missing {label}")
    return match.group(1)

name = required(r'Property::Name\("([^"]*)"\)', build, "PiPL name")
category = required(r'Property::Category\("([^"]*)"\)', build, "PiPL category")
match_name = required(r'Property::AE_Effect_Match_Name\("([^"]*)"\)', build, "PiPL match name")
support = required(r'Property::AE_Effect_Support_URL\("([^"]*)"\)', build, "PiPL support URL")
major = required(r'PF_PLUG_IN_VERSION:\s*u16\s*=\s*(\d+)', build, "PiPL API major")
minor = required(r'PF_PLUG_IN_SUBVERS:\s*u16\s*=\s*(\d+)', build, "PiPL API minor")

callback_start = shell.find("const A_Err result = in_callback(")
if callback_start < 0:
    raise SystemExit("missing shell registration callback")
callback_end = shell.find(");", callback_start)
if callback_end < 0:
    raise SystemExit("unterminated shell registration callback")
callback = shell[callback_start:callback_end]

literals = re.findall(
    r'reinterpret_cast<const std::uint8_t\*>\("([^"]*)"\)',
    callback,
)
expected_literals = [name, match_name, category, "EffectMain", support]
if literals[:5] != expected_literals:
    raise SystemExit(
        f"shell registration metadata drift:\n"
        f"  expected={expected_literals!r}\n"
        f"  actual={literals[:5]!r}"
    )

shell_major = required(r'kApiMajor\s*=\s*(\d+)', shell, "shell API major")
shell_minor = required(r'kApiMinor\s*=\s*(\d+)', shell, "shell API minor")
if (shell_major, shell_minor) != (major, minor):
    raise SystemExit(
        f"shell API version drift: build={major}.{minor} shell={shell_major}.{shell_minor}"
    )

print(
    f"shell metadata: PASS name={name!r} category={category!r} "
    f"match={match_name!r} api={major}.{minor}"
)
