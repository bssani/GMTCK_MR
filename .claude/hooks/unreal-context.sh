#!/usr/bin/env bash
# SessionStart hook: inject a short note identifying the session as operating inside
# an Unreal Engine project, plus the few project facts that are costly to rediscover.
#
# Walks upward from $PWD so sessions started in subdirectories (Source/, Content/,
# Plugins/, ...) are still detected. Every appended line is gated on a signal we
# actually detected, so this stays correct when the template is cloned per program.
#
# Adapted from Epic Games' unreal-engine-skills-for-claude-code-plugin (MIT).
# Opt-in debug logging: set CLAUDE_UE_HOOK_DEBUG to any non-empty value.

debug() {
  if [ -n "$CLAUDE_UE_HOOK_DEBUG" ]; then
    echo "unreal-context.sh: $*" >&2
  fi
}

is_project_root() {
  # Reliable top-level markers only. A bare `Engine` directory is NOT a marker:
  # the Unreal source tree contains an `Engine` module at
  # `Engine/Source/Runtime/Engine`, so walking up from a Runtime module would
  # otherwise short-circuit and mis-identify it as the root.
  local directory="$1"
  [ -f "$directory/GenerateProjectFiles.bat" ] && return 0
  [ -f "$directory/GenerateProjectFiles.sh" ] && return 0
  for candidate in "$directory"/*.uproject; do
    [ -e "$candidate" ] && return 0
  done
  return 1
}

find_project_root() {
  local directory="$1"
  while [ -n "$directory" ]; do
    if is_project_root "$directory"; then
      echo "$directory"
      return 0
    fi
    local parent
    parent="$(dirname "$directory")"
    [ "$parent" = "$directory" ] && break
    directory="$parent"
  done
  return 1
}

project_root="$(find_project_root "$PWD")"
if [ -z "$project_root" ]; then
  debug "no Unreal Engine project marker found walking up from $PWD"
  exit 0
fi

uproject_filename=""
for candidate in "$project_root"/*.uproject; do
  if [ -e "$candidate" ]; then
    uproject_filename="$(basename "$candidate")"
    break
  fi
done

# Engine version straight from the .uproject, so this never goes stale.
engine_version=""
if [ -n "$uproject_filename" ]; then
  engine_version="$(sed -n 's/.*"EngineAssociation"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
    "$project_root/$uproject_filename" 2>/dev/null | head -1)"
fi

target_name=""
for candidate in "$project_root"/Source/*.Target.cs; do
  if [ -e "$candidate" ]; then
    target_name="$(basename "$candidate" .Target.cs)"
    break
  fi
done

cxmr_present="false"
[ -d "$project_root/Plugins/CXMR" ] && cxmr_present="true"

agent_present="false"
[ -d "$project_root/Plugins/UnrealAgent" ] && agent_present="true"

debug "root=$project_root uproject=$uproject_filename engine=$engine_version cxmr=$cxmr_present agent=$agent_present"

# --- Build the injected context. Keep it short. ---
context="This working directory is an Unreal Engine project"
[ -n "$uproject_filename" ] && context="$context (\`$uproject_filename\`"
[ -n "$uproject_filename" ] && [ -n "$engine_version" ] && context="$context, engine $engine_version"
[ -n "$uproject_filename" ] && context="$context)"
context="$context. Prefer Unreal Engine conventions (C++/UObject patterns, Slate, UHT reflection) when suggesting code."

if [ "$cxmr_present" = "true" ]; then
  context="$context This is the CXMR mixed-reality vehicle-review template (Varjo XR-4): the portable core lives in"
  context="$context \`Plugins/CXMR/\` and must not depend on project content. Asset paths: \`/CXMR/...\` is plugin content,"
  context="$context \`/Game/...\` is project content -- always state which one you mean."
fi

if [ -n "$target_name" ]; then
  context="$context Build with \`Engine/Build/BatchFiles/Build.bat ${target_name}Editor Win64 Development -Project=<abs .uproject>\`."
fi

# The single most expensive mistake in this repo: git operations while the editor holds .uasset locks.
context="$context Close the Unreal Editor before any git branch switch, merge, or C++ rebuild -- the editor locks"
context="$context \`.uasset\` files and a mid-operation failure leaves the working tree half-written."

if [ "$agent_present" = "true" ]; then
  context="$context The UnrealAgent plugin provides an in-editor MCP server (editor ops via Python and toolsets);"
  context="$context it is only reachable while the editor is running, so C++ rebuilds and editor-driven work cannot overlap."
fi

# JSON-escape the dynamic content: backslash first, then double quote.
escaped_context="${context//\\/\\\\}"
escaped_context="${escaped_context//\"/\\\"}"

printf '{"hookSpecificOutput":{"hookEventName":"SessionStart","additionalContext":"%s"}}\n' "$escaped_context"
