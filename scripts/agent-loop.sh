#!/usr/bin/env bash
set -euo pipefail

readonly EXIT_OK=0
readonly EXIT_ERROR=1
readonly EXIT_BLOCKED=2
readonly EXIT_ITERATION_LIMIT=3
readonly EXIT_TIMEOUT=124

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
readonly SCRIPT_DIR REPO_ROOT

readonly TASKS_FILE="${REPO_ROOT}/TASKS.md"
readonly AGENTS_FILE="${REPO_ROOT}/AGENTS.md"
readonly RUNTIME_DIR="${REPO_ROOT}/.agent-runtime"
readonly LOOP_LOG="${RUNTIME_DIR}/agent-loop.log"
readonly LOCK_FILE="${RUNTIME_DIR}/agent-loop.lock"
readonly LAST_OUTPUT="${REPO_ROOT}/.codex-last.txt"

readonly MAX_ITERATIONS="${AGENT_MAX_ITERATIONS:-20}"
readonly MAX_SAME_TASK_REPEATS="${AGENT_MAX_SAME_TASK_REPEATS:-2}"
readonly CODEX_TIMEOUT="${AGENT_CODEX_TIMEOUT:-90m}"
readonly CODEX_BIN="${CODEX_BIN:-codex}"
readonly DRY_RUN="${AGENT_DRY_RUN:-0}"

log() {
    printf '%s\n' "$*" | tee -a "${LOOP_LOG}"
}

die() {
    log "ERROR: $*"
    exit "${EXIT_ERROR}"
}

require_positive_integer() {
    local name="$1"
    local value="$2"
    [[ "${value}" =~ ^[1-9][0-9]*$ ]] ||
        die "${name} must be a positive integer (got: ${value})"
}

validate_task_ids() {
    local duplicates
    duplicates="$(awk '
        /^- \[[ x]\] [[:alnum:]][[:alnum:]-]*([[:space:]]|$)/ {
            line = $0
            sub(/^- \[[ x]\] /, "", line)
            split(line, fields, /[[:space:]]+/)
            count[fields[1]]++
        }
        END {
            for (id in count)
                if (count[id] > 1) print id
        }
    ' "${TASKS_FILE}" | sort)"
    [[ -z "${duplicates}" ]] ||
        die "duplicate task ID(s) in TASKS.md: ${duplicates//$'\n'/, }"
}

first_pending_line() {
    awk '/^- \[ \] [[:alnum:]][[:alnum:]-]*([[:space:]]|$)/ { print; exit }' \
        "${TASKS_FILE}"
}

extract_task_block() {
    local task_id="$1"
    awk -v id="${task_id}" '
        $0 ~ "^- \\[([ x])\\] " id "([[:space:]]|$)" { selected = 1 }
        selected && seen && /^- \[[ x]\] [[:alnum:]][[:alnum:]-]*([[:space:]]|$)/ { exit }
        selected { print; seen = 1 }
    ' "${TASKS_FILE}"
}

task_occurrences() {
    local task_id="$1"
    awk -v id="${task_id}" '
        $0 ~ "^- \\[([ x])\\] " id "([[:space:]]|$)" { count++ }
        END { print count + 0 }
    ' "${TASKS_FILE}"
}

block_has_real_evidence() {
    awk '
        /^  - Evidence:[[:space:]]*$/ { evidence = 1; next }
        evidence && /^  - [[:alnum:]][[:alnum:] /_-]*:[[:space:]]*$/ { evidence = 0 }
        evidence {
            value = $0
            sub(/^[[:space:]]*-[[:space:]]*/, "", value)
            sub(/[[:space:]]+$/, "", value)
            lower = tolower(value)
            if (value != "" && lower != "pending" && lower != "pending.") found = 1
        }
        END { exit found ? 0 : 1 }
    '
}

validate_completed_tasks() {
    local task_id task_block
    while IFS= read -r task_id; do
        [[ -n "${task_id}" ]] || continue
        task_block="$(extract_task_block "${task_id}")"
        grep -Eq '^  - Status: DONE[[:space:]]*$' <<<"${task_block}" ||
            die "${task_id} is checked but lacks Status: DONE"
        block_has_real_evidence <<<"${task_block}" ||
            die "${task_id} is checked but lacks real Evidence"
    done < <(awk '
        /^- \[x\] [[:alnum:]][[:alnum:]-]*([[:space:]]|$)/ {
            line = $0
            sub(/^- \[x\] /, "", line)
            split(line, fields, /[[:space:]]+/)
            print fields[1]
        }
    ' "${TASKS_FILE}")
}

next_iteration_number() {
    local path base number max=0
    shopt -s nullglob
    for path in "${RUNTIME_DIR}"/iteration-*.prompt.txt \
                "${RUNTIME_DIR}"/iteration-*.log; do
        base="${path##*/iteration-}"
        number="${base%%.*}"
        if [[ "${number}" =~ ^[0-9]+$ ]] && (( 10#${number} > max )); then
            max=$((10#${number}))
        fi
    done
    shopt -u nullglob
    printf '%d\n' "$((max + 1))"
}

write_prompt() {
    local prompt_file="$1"
    local task_line="$2"
    local task_id="$3"
    cat >"${prompt_file}" <<EOF
Work inside this repository only:

${REPO_ROOT}

Read AGENTS.md and TASKS.md first.

The external agent loop selected exactly this task:

${task_id}: ${task_line}

Work on THIS TASK ONLY.

Do not start the next pending task.

Required behavior:

1. Inspect repository, implementation, tests, CMake and Git state.
2. Follow AGENTS.md.
3. Read the selected task's description, acceptance criteria and tests.
4. Perform the work; do not merely explain it.
5. Add/update regression tests where behavior changes.
6. Run directly relevant tests.
7. Run appropriate regression tests.
8. Fix ordinary implementation/compiler/test failures.
9. Preserve unrelated user changes.
10. Do not modify files outside the repository.
11. Do not commit.
12. Do not push.
13. Do not run destructive Git commands.
14. Run git diff --check.
15. Inspect the final diff.
16. Update only the selected task's Status/Evidence.
17. Mark [x] only after objective acceptance verification.
18. If truly blocked, leave [ ], set Status: BLOCKED, record evidence and finish the response with:
BLOCKED:
19. Otherwise set Status: DONE, add concise evidence, mark [x], and stop.

Do not continue to another TASKS.md task.
EOF
}

require_positive_integer AGENT_MAX_ITERATIONS "${MAX_ITERATIONS}"
require_positive_integer AGENT_MAX_SAME_TASK_REPEATS "${MAX_SAME_TASK_REPEATS}"
[[ "${DRY_RUN}" == "0" || "${DRY_RUN}" == "1" ]] ||
    die "AGENT_DRY_RUN must be 0 or 1"
[[ -f "${AGENTS_FILE}" ]] || die "AGENTS.md not found at repository root"
[[ -f "${TASKS_FILE}" ]] || die "TASKS.md not found at repository root"

mkdir -p "${RUNTIME_DIR}"

if command -v flock >/dev/null 2>&1; then
    exec 9>"${LOCK_FILE}"
    flock -n 9 || die "another agent loop already holds ${LOCK_FILE}"
else
    log "WARNING: flock is unavailable; concurrent loop protection is disabled"
fi

validate_task_ids
validate_completed_tasks

iteration=0
same_task_repeats=0
previous_task_id=""
runtime_sequence="$(next_iteration_number)"

while (( iteration < MAX_ITERATIONS )); do
    task_line="$(first_pending_line)"
    if [[ -z "${task_line}" ]]; then
        log "All queued tasks are complete."
        exit "${EXIT_OK}"
    fi

    task_id="$(sed -E 's/^- \[ \] ([[:alnum:]-]+).*/\1/' <<<"${task_line}")"
    [[ "${task_id}" =~ ^[[:alnum:]][[:alnum:]-]*$ ]] ||
        die "cannot parse task ID from: ${task_line}"
    [[ "$(task_occurrences "${task_id}")" == "1" ]] ||
        die "selected task ${task_id} does not appear exactly once"

    task_block="$(extract_task_block "${task_id}")"
    [[ -n "${task_block}" ]] || die "cannot extract block for ${task_id}"
    if grep -Eq '^- \[x\] ' <<<"${task_block}"; then
        die "first pending selection for ${task_id} produced a completed block"
    fi

    if [[ "${task_id}" == "${previous_task_id}" ]]; then
        same_task_repeats=$((same_task_repeats + 1))
    else
        same_task_repeats=0
        previous_task_id="${task_id}"
    fi

    printf -v sequence_label '%03d' "${runtime_sequence}"
    prompt_file="${RUNTIME_DIR}/iteration-${sequence_label}.prompt.txt"
    iteration_log="${RUNTIME_DIR}/iteration-${sequence_label}.log"
    write_prompt "${prompt_file}" "${task_line}" "${task_id}"

    if [[ "${DRY_RUN}" == "1" ]]; then
        log "DRY RUN: repository=${REPO_ROOT}"
        log "DRY RUN: selected=${task_id}"
        log "DRY RUN: prompt=${prompt_file}"
        log "DRY RUN: would run ${CODEX_BIN} --ask-for-approval never exec --cd ${REPO_ROOT} --sandbox workspace-write --output-last-message ${LAST_OUTPUT} -"
        exit "${EXIT_OK}"
    fi

    command -v timeout >/dev/null 2>&1 ||
        die "GNU timeout is required before starting Codex"
    timeout_version="$(timeout --version 2>/dev/null)"
    grep -q 'GNU coreutils' <<<"${timeout_version}" ||
        die "GNU timeout is required before starting Codex"
    command -v "${CODEX_BIN}" >/dev/null 2>&1 ||
        die "Codex executable not found: ${CODEX_BIN}"

    codex_args=()
    if [[ -n "${CODEX_EXEC_ARGS:-}" ]]; then
        # Intentionally simple: whitespace-separated arguments only; shell quoting
        # and embedded whitespace are not interpreted.
        read -r -a codex_args <<<"${CODEX_EXEC_ARGS}"
        for extra_arg in "${codex_args[@]}"; do
            case "${extra_arg}" in
                --full-auto|--dangerously-bypass-approvals-and-sandbox|\
                --dangerously-bypass-hook-trust|danger-full-access|\
                --sandbox|--sandbox=*|-s|-s=*|\
                --ask-for-approval|--ask-for-approval=*|-a|-a=*|\
                --cd|--cd=*|-C|-C=*|--add-dir|--add-dir=*|\
                --output-last-message|--output-last-message=*|-o|-o=*)
                    die "CODEX_EXEC_ARGS may not override safety/runtime option: ${extra_arg}"
                    ;;
            esac
        done
    fi

    before_hash="$(sha256sum "${TASKS_FILE}" | awk '{ print $1 }')"
    : >"${LAST_OUTPUT}"
    log "Starting ${task_id} (iteration $((iteration + 1))/${MAX_ITERATIONS})"

    set +e
    timeout --foreground "${CODEX_TIMEOUT}" \
        "${CODEX_BIN}" --ask-for-approval never exec \
        --cd "${REPO_ROOT}" \
        --sandbox workspace-write \
        --output-last-message "${LAST_OUTPUT}" \
        "${codex_args[@]}" \
        - <"${prompt_file}" 2>&1 | tee "${iteration_log}"
    pipeline_status=("${PIPESTATUS[@]}")
    set -e
    codex_status="${pipeline_status[0]}"
    tee_status="${pipeline_status[1]}"

    if [[ "${codex_status}" == "${EXIT_TIMEOUT}" ]]; then
        log "Codex timed out after ${CODEX_TIMEOUT}"
        exit "${EXIT_TIMEOUT}"
    fi
    [[ "${tee_status}" == "0" ]] || die "tee failed with exit ${tee_status}"

    validate_task_ids
    validate_completed_tasks
    [[ "$(task_occurrences "${task_id}")" == "1" ]] ||
        die "selected task ${task_id} disappeared or changed identity"
    task_block="$(extract_task_block "${task_id}")"
    [[ -n "${task_block}" ]] || die "selected task ${task_id} disappeared"

    if grep -Eq '^  - Status: BLOCKED[[:space:]]*$' <<<"${task_block}" ||
       grep -Eq '^BLOCKED:' "${LAST_OUTPUT}"; then
        log "Task ${task_id} requires human input."
        exit "${EXIT_BLOCKED}"
    fi

    [[ "${codex_status}" == "0" ]] ||
        die "Codex exited with status ${codex_status} while processing ${task_id}"

    after_hash="$(sha256sum "${TASKS_FILE}" | awk '{ print $1 }')"
    if [[ "${before_hash}" == "${after_hash}" ]]; then
        die "Codex returned success without changing TASKS.md for ${task_id}"
    fi

    if grep -Eq "^- \\[x\\] ${task_id}([[:space:]]|$)" <<<"${task_block}"; then
        grep -Eq '^  - Status: DONE[[:space:]]*$' <<<"${task_block}" ||
            die "${task_id} is checked but lacks Status: DONE"
        block_has_real_evidence <<<"${task_block}" ||
            die "${task_id} is checked but lacks real Evidence"
        log "Task ${task_id} completed with objective evidence."
    elif grep -Eq "^- \\[ \\] ${task_id}([[:space:]]|$)" <<<"${task_block}"; then
        grep -Eq '^  - Status: PENDING[[:space:]]*$' <<<"${task_block}" ||
            die "${task_id} remains unchecked without Status: PENDING or BLOCKED"
        if (( same_task_repeats >= MAX_SAME_TASK_REPEATS )); then
            die "${task_id} remains pending after ${MAX_SAME_TASK_REPEATS} repeat(s)"
        fi
        log "Task ${task_id} remains pending after a recorded TASKS.md update; retrying."
    else
        die "${task_id} has an invalid checkbox state"
    fi

    iteration=$((iteration + 1))
    runtime_sequence=$((runtime_sequence + 1))
done

validate_task_ids
validate_completed_tasks
if [[ -z "$(first_pending_line)" ]]; then
    log "All queued tasks are complete."
    exit "${EXIT_OK}"
fi

log "Iteration limit ${MAX_ITERATIONS} reached with pending tasks."
exit "${EXIT_ITERATION_LIMIT}"
