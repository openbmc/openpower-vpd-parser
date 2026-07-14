# Code Review and Integration Process

This document outlines how code reviews, testing, approvals, and integrations
are handled in this repository. We follow the official OpenBMC workflow using
Gerrit for code submission and Jenkins for continuous integration.

## ⏱️ Timeline and Lifespan

- Minimum Review Period: All patches must remain open for at least 2 days (48
  hours) to allow global contributors across different time zones to review the
  changes.
- Exceptions: Critical security patches or emergency build fixes may be
  fast-tracked by a repository maintainer.

## ð What to Review

Reviewers must evaluate submissions against the following core areas:

## 1. Code Quality & Standards

- C++ Repositories: Code must adhere strictly to the OpenBMC C++ Style Guide
  (based on clang-format and cppcheck).
- Python Repositories: Code must pass flake8, black, and conform to PEP 8
  standards.
- JSON Repositories & Configurations: Files must be valid JSON, properly
  formatted (typically 4-space indentation), and must validate successfully
  against relevant JSON schemas (e.g., entity-manager schemas, Redfish schemas,
  or project-specific validation scripts).
- Commit Messages: Must follow the 50/72 rule (50-character summary, blank line,
  72-character body wrapping) and clearly explain why the change is necessary.

## 2. Required Elements

- Developer Certificate of Origin (DCO): Every commit message must include a
  valid `Signed-off-by: Real Name <email>` line matching the author.
- Testing Documentation: The commit message body must include a `Testing:`
  section detailing how the change was verified (e.g., QEMU run, hardware logs,
  unit test output).

## ✅ Approval Requirements for Integration

A patch cannot be merged until it receives the following score matrix in Gerrit:

- Verified +1: Automatically granted by the Jenkins CI system after passing unit
  tests (make check) and integration builds.
- Code-Review +1: Granted by peers, community members, or domain experts
  validating the logic. At least one +1 from a peer is highly encouraged for all
  non-trivial changes.
- Code-Review +2: Granted exclusively by a designated Repository Maintainer
  listed in the OWNERS file. This score signals final architectural approval and
  unlocks the ability to merge.

## ð Integration Workflow

1. Submit: Developer pushes a patchset via `git review`.
2. Test: Jenkins triggers and reports a Verified score.
3. Review: Peers review, leave inline comments, and score +1.
4. Approve: Maintainer reviews, addresses community feedback, and scores +2.
5. Merge: The maintainer or automated submission queue submits the patch to the
   master branch.

## ð¤ AI-Assisted Development Policy

The use of Artificial Intelligence (AI) tools (e.g., GitHub Copilot, ChatGPT,
Anthropic Claude) for writing code, generating unit tests, or drafting commit
messages is permitted under the following strict conditions:

## 1. Ultimate Developer Accountability

- Ownership: The human developer who signs off on the commit (`Signed-off-by`)
  is 100% responsible for the code.
- Validation: You must fully understand, manually trace, and test every line of
  AI-generated code before submitting it. Saying "the AI generated it this way"
  is not an acceptable response to review findings or subsequent production
  bugs.

## 2. Mandatory Disclosures

To maintain transparency and alert reviewers to look closer at potential
algorithmic hallucinations, you must explicitly disclose AI usage in your commit
message.

- Format: Append a note at the very bottom of your commit message body (just
  above the `Signed-off-by` line).
- Examples:
  - `Assisted-by: AI (GitHub Copilot for boilerplate structure)`
  - `Commit-message-drafted-by: OpenAI ChatGPT`

## 3. Intellectual Property & Code Security

- Proprietary Data: Never paste proprietary, confidential, or internal OpenBMC
  vendor keys/code into public commercial LLMs.
- Licensing Compliance: Be vigilant about AI models suggesting large,
  copy-pasted blocks of code that might violate open-source licenses (e.g., GPL
  compliance in firmware). Ensure all output aligns with the project's Apache
  2.0 or GPLv2 licenses.
- Security Auditing: AI frequently generates insecure code patterns (such as
  buffer overflows or weak cryptographic implementations). Pay extra attention
  to boundary conditions and memory management in C++ submissions.
