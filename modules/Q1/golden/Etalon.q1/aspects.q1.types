namespace Q1_iQSM
  namespace Etalon

    //@ in is not forward, it is mature Aspect definition
    entity Trivia

    entity SampleEntity
      always
        max_elements: integer = 2000
        absolute_min: integer = -1000 // static constants are always defined
        absolute_max: integer = 1000
        ?from_float(float) -> integer
      one
        data_field: integer
        ?const_element_method() -> string
        =nonconst_element_method()
        !min_value()
        !max_value()
      all
        common_data: integer
        >from_float(value_approximate: float) -> #
        ?find_first(integer) -> #?
        ?const_fieldwide_method() -> integer // just sum of all values
        =nonconst_fieldwide_method() // removes one item with minimal value
        !some_logic_fieldwide_invariant()

    attribute Tag of SampleEntity
      all
        modulus: integer
        !modulus_clamped() // must be >= 1
        !q1::existence(needs_tag) // appears when SampleEntity.data_field is divided by modulus

    component Remnant of Tag
      one
        power: integer @cache// == SampleEntity::data_field / Tag::Modulus (Tag quarantees integer)
        !renmant_calculated()

    component SampleComponent of SampleEntity
      one
        =example_op_multiply(factor: integer)
        =example_op_div_with_remainder(divisor: integer) -> integer

    attribute SampleAttribute of SampleEntity
      one
        main_anchor: anchor<Trivia>
        main_dummy: control<Trivia>
      all
        // TODO: rework syntax and even conception of "constructor"
        >complex_constructor(existing: #SampleEntity) -> #

    // experimental one-line form of syntax (sorry, parser!)
    entity Note one text: string

    group<Note> of SampleEntity

    // Hint: '=' functions aka "item modifiers" are meaningless for Archetype (has no own state/quantum)
    archetype Notebook
      ?notes_count(#SampleEntity)->integer
      >add_note(#SamleEntity, ~Note::text)-> #Note
