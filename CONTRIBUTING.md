# Contributing to NexCache

Welcome! Thank you for your interest in contributing to NexCache. This project aims to push the boundaries of in-memory database performance, and your help is appreciated.

## How to Get Involved

### 1. Discuss Before Coding
For major features, architectural changes, or new APIs, please open a **GitHub Issue** first. This allows us to discuss the proposal, use cases, and technical alignment with the NexCache core engine before you invest time in coding.

### 2. Submission Process
1. **Fork** the repository on GitHub.
2. Create a **Topic Branch** (`git checkout -b feature/my-cool-feature`).
3. Make your changes, ensuring they follow the [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md).
4. **Sign-off** your commits. We use the Developer Certificate of Origin (DCO) to ensure contributions are correctly licensed. Use `git commit -s` to add the `Signed-off-by` line.
5. Push to your branch and open a **Pull Request**.

### 3. Developer Certificate of Origin (DCO)
By signing off your commits, you certify that:
- The contribution was created by you.
- You have the right to submit it under the BSD-3-Clause license.
- You understand the contribution will be public.

## Technical Quality
All contributions are expected to:
- Maintain high performance (sub-microsecond latency where applicable).
- Be memory-efficient (respecting the NexDash density goals).
- Include appropriate tests (see `tests/` directory).

## Contact
For private inquiries or specific proposals, you can reach the lead maintainer at **giuseppelobbene@gmail.com**.

---
*NexCache — Rethink Memory, Accelerate AI.*
