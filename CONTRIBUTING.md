# Contributing to DZSTUDIO

Thanks for helping. DZSTUDIO is a public beta, so bug reports are just as valuable as code.

## Reporting a bug

Open an issue with:

- what you did, what you expected and what happened instead
- when you last updated DZSTUDIO from the Workshop, and the map
- a screenshot, and for errors or crashes the newest `script_*.log` and `*.RPT` from `%LOCALAPPDATA%\DayZ`

Questions and ideas are also welcome in [#dzstudio on Discord](https://discord.gg/fxyev4w9yZ).

## Pull requests

- Keep a pull request to one change, and say in its description what it changes for someone using DZSTUDIO.
- Follow the code around you: names start with `DZS_`, the core stays small, and features are plugins on the public
  API. If a plugin needs something the API does not offer, extend the API rather than reaching into the core.
- Every change to a project goes through an operation, so it can be undone and shared in a session.
- DZSTUDIO only offers what works on a console server. A feature whose result a console server cannot load does not
  belong in it.
- Test your change in the game (`DayZ_x64.exe "-mod=@dzstudio"`) and say how you tested it.

## Sign-off (DCO)

Contributions are accepted under the [Developer Certificate of Origin 1.1](https://developercertificate.org/): by
signing off a commit you confirm that you wrote it or otherwise have the right to submit it under the project's
license. Sign off every commit:

```bash
git commit -s -m "Fix the brush radius on slopes"
```

which adds a line `Signed-off-by: Your Name <you@example.org>` to the message. A check on every pull request looks for
it.

## License

By contributing you agree that your contribution is licensed under the [Mozilla Public License 2.0](LICENSE), like the
rest of the repository.
