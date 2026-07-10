# Q1 Etalon (aspects.q1.types)

`attribute A of Host` — attribute aspect parasitic on `Host`.

`feature F of Host` — feature aspect parasitic on `Host`.
```
namespace Q1_fQSM
  namespace Etalon

    //@ in is not forward, it is mature Aspect definition (just minimal Aspect)
    entity Trivia

    entity SampleEntity
      always //@ treat as public static const (constexpr) in the Aspect root
        max_elements: integer = 2000 // static constants values must be always defined
        absolute_min: integer = -1000
        absolute_max: integer = 1000
        ?from_float(float) -> integer //@ always-level functions have no additional parameters
      one //@ item-level data and oprtaions
        data_field: integer
        ?const_element_method() -> string
        =nonconst_element_method()
        !min_value(=one) // quantum constraint on change
        !max_value(=one)
      all
        common_data: integer
        >from_float(value_approximate: float) -> #
        ?find_first(integer) -> #?
        ?const_fieldwide_method() -> integer // just sum of all values
        =nonconst_fieldwide_method() // removes one item with minimal value
        !some_logic_fieldwide_invariant(~) // aspect-wide on own type

    attribute Tag of SampleEntity
      all
        modulus: integer
        !modulus_clamped(~) // must be >= 1

    component Remnant of Tag
      one
        power: integer @cache// == SampleEntity::data_field / Tag::Modulus (Tag quarantees integer)
        // NOT IMPLEMENTED as Q1 feature: !sync(>one)
      all
        !sync(~Tag) //@

    component SampleComponent of SampleEntity
      one
        =example_op_multiply(factor: integer)
        =example_op_div_with_remainder(divisor: integer) -> integer

    attribute SampleAttribute of SampleEntity
      one
        main_anchor: anchor<Trivia> //@ any anchor<> / contro<> leads to custom Internals to add achnor/conrol behavior
        main_dummy: control<Trivia>
      all
        // TODO: rework syntax and even conception of "constructor"
        >complex_constructor(existing: #SampleEntity) -> #
        !limit_by_tag_count(~Tag)

    // experimental one-line form of syntax (sorry, parser!)
    entity Note one text: string

    group<Note> of SampleEntity

    // Hint: '=' functions aka "item modifiers" are meaningless for Archetype (has no own state/quantum)
    archetype Notebook
      ?notes_count(#SampleEntity)->integer
      >add_note(#SampleEntity, ~Note::text)-> #Note
  ```
