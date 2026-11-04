---
name: mainverte-code-review
description: Review MainVerte pull requests, commits, branch diffs or completed implementations for correctness, accidental complexity and maintainability. Review-only; do not implement fixes.
---

# MainVerte code review

Review changes as a maintainer responsible for understanding,
debugging and evolving the implementation.

Apply the repository's L.O.C.A.L. principles.
Prioritize concrete evidence over general design preferences.

Do not modify code, commit, push or create pull requests.

## Establish scope

- Read the applicable `AGENTS.md` instructions.
- Identify the intended behavior and the review base.
- Examine the complete relevant diff and surrounding integration points.
- Trace affected callers, state transitions and ownership boundaries when necessary.
- Do not infer requirements solely from the implementation. Disclose missing context or uncertainty.

## Review priorities

### Correctness

Look for concrete defects, particularly:

- Violated business invariants or incorrect observable behavior.
- Ownership, lifetime, thread-affinity or callback errors.
- Persistence inconsistencies, broken transactions or migration risks.
- UI state inconsistent with Core operation results.
- Error paths that silently fail or leave invalid state.
- Regressions in existing behavior.

Explain the failure mechanism before reporting a defect.

### Complexity and maintainability

Apply L.O.C.A.L. to evaluate whether the implementation
is proportionate to the actual requirements.

Look for:

- Abstractions or generic infrastructure without demonstrated value.
- Unnecessary indirection or fragmented control flow.
- Redundant state, synchronization or defensive mechanisms.
- Duplication of mechanisms already available in the project.
- Speculative extensibility or unrelated refactoring.
- Changes that make behavior harder to understand or modify locally.

Recommend simplification only when a concrete alternative
reduces complexity without sacrificing correctness.

Do not favor abstraction merely to remove small local duplication.
Do not remove necessary safety or lifetime safeguards for brevity.

### Verification

Assess relevant tests and validation evidence.
Identify significant coverage gaps and unverified platform behavior.

Distinguish observed defects from suspected risks.
Never claim tests were executed unless they actually ran.

## Findings

Report only actionable findings.

For each finding provide:

- Severity: Critical, High, Medium or Low.
- Category: Correctness, Complexity or Maintainability.
- Location: precise file and line or function.
- Evidence: concrete failure mechanism or unnecessary cost.
- Recommendation: smallest practical correction.

Separate correctness defects from optional simplifications.
Ignore subjective style preferences handled by tooling.
Do not generate findings to fill a quota.

## Assessment

Conclude briefly with:

- Blocking defects and worthwhile simplifications, if any.
- Overall complexity relative to the requested behavior.
- Key files or functions requiring maintainer attention.
- Verification gaps and remaining uncertainty.

If no significant findings exist, explicitly say so.