# Security policy

## Reporting a vulnerability

Please report security problems privately, not in a public issue. Examples: a crafted `.groom`,
Alembic, USD or graph file that crashes GroomLab or runs code; something wrong with the update
check or the Blender helper's pipes and shared memory.

- Email **support@getgroomlab.com** with "Security" in the subject.

Include the GroomLab version, the host, and the steps or a file that reproduces the problem. We
aim to reply within a week and will credit you in the release notes unless you'd rather not be
named.

## Supported versions

Only the latest release gets security fixes.

## Scope notes

- GroomLab's only network access is the optional update check: one GET of a public `latest.json`
  over HTTPS. It sends nothing else.
- Expressions in groom graphs are evaluated by GroomLab's own small expression language, not by
  Python. A graph or `.groom` file that leads to code execution is a vulnerability. The Maya
  Python API and presets you run yourself are not.
