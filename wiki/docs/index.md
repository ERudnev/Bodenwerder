# Bodenwerder

*Private research workshop*

 - I think in different language; translating into English is not hard, but keeping everything in two languages is. The blog, informal thoughts, and many of the harder definitions (which get revised from time to time) stay in my mother tongue.
 - Some ideas here may look uncommon; some terms do too. I am looking for the best words that don't lie, every day. Sorry in advance if some terms do not look final — that's because they are not.

An experimental workspace for describing, executing, and exploring quantized world models.

**Ideas:**

- Data first, operations later.
- Any system is just a `map<type, map<Id, Value>>`.
- Any change is just a `map<type, map<Id, Value>>`.
- Any operation is just a function `(Context, type, identity) -> change`.

## How this documentation is organized

- **Blog** — [blog/](blog/)
- **Ideas and debt** — [tech_stuff/](tech_stuff/)
- **Q1** — language for model definitions: [aspects etalon](Q1/aspects.q1.types.md)
- **Aeris** — proof-of-concept game for this codebase: [reading](Aeris/reading/fragments/aeris.md), [engineering](Aeris/engineering/design_tech.md)

## Status

Active research and development. Concepts, APIs, naming, and implementation details are expected to change as the system matures.
