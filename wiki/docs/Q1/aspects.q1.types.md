# Q1 Etalon (aspects.q1.types)

Authoritative file: `modules/Q1/golden/doctrine/aspects.q1.types`.

Reaction vocabulary (watch / action / reflex, `->>` / `->=`, one=object / all=set): see [reaction_is_effector.md](reaction_is_effector.md) and `modules/Q1/syntax.txt`.

Template operations (`?name<R as …>(…) -> #R?`): see [template_operations.md](template_operations.md).

Snapshot of current doctrine (abbreviated):

```
namespace Q1_fQSM
  namespace Etalon

    entity Trivia

    entity SampleEntity
      always
        max_elements: integer = 2000
        absolute_min: integer = -1000
        absolute_max: integer = 1000
        ?from_float(float) -> integer
      one
        data_field: integer
        !min_value(=one)
        !max_value(=one)
      all
        common_data: integer
        >from_float(value_approximate: float) -> #
        !some_logic_fieldwide_invariant(~) // set-as-set

    entity Clock
      one
        current: time

    entity ReactionSketch
      one
        value: string
        !normalize(=one)
        !watch_clock(~Clock)->=field_update()
        !watch_sample(~SampleEntity)->>reflex()
        !watch_trigger(~SampleEntity)->>field_action()
      all
        >field_action()

    attribute Tag of SampleEntity
      all
        modulus: integer
        !modulus_clamped(~) // set-as-set (Global)

    entity Reminder
      one
        target: affects<SampleEntity>
        !remove_after_happened(~)
        !write_log_when_reached(~SampleEntity)

    component Remnant of Tag
      one
        power: integer @cache
        !sync(~Tag)

    attribute SampleAttribute of SampleEntity
      one
        main_anchor: anchor<Trivia>
        main_dummy: custody<Trivia>
      all
        !limit_by_tag_count(~Tag) // set-as-set
```
