# FQSM Model Glossary

This document describes the conceptual vocabulary used within the FQSM model layer. These terms are not intended to be interpreted as direct analogues of their physical or mathematical counterparts. Physical and mathematical terminology is used as a source of intuition and structural analogy.


## Realm

A realm is a complete, self-consistent state space together with its evolution rules.
A realm is the primary persistent object of the system.
A realm contains states and produces transitions between states. Auxiliary objects such as drafts, previews, deltas, patches, and bosons may participate in the evolution process but are not considered part of the realm itself.
A realm is what can be stored, loaded, serialized, replicated, or rendered.

---

## Quantum

A quantum is a structural description of information.
A quantum defines the shape of data independently of its role.
The same quantum may be used to represent:

- persistent state,
- transient state,
- modifications,
- deltas,
- operational data.

A quantum is a form, not an object.

---

## Particle

A particle is an instantiated quantum.
Particles live inside linear structures.
A particle represents a single aspect of a larger object.

Examples:

- Body particle
- Physics particle
- Mind particle
- Render particle

Particles are persistent members of the realm state.

---

## Atom

_Atoms are not "atomic"_

An atom is a composition of particles sharing the same object identity.
An atom is formed by combining multiple particles from different lines.
Example:

Entity "Elephant" may consist of:

- Body particle
- Physics particle
- Mind particle
- Render particle

Together they form a single atom representing the elephant.
Atoms are conceptual objects reconstructed from the model.

---

## Boson

A boson is a transient quantum participating in state transition.
A boson represents change rather than state.
Bosons are produced during evaluation and integration.
Bosons may carry information required to transform state A into state A'.
Bosons are generally not considered part of persistent realm state.
Current implementation maps bosons to patch-like structures.
This term is intentionally conceptual and may evolve over time.

---

## Dimensions
Axis of model

### Elementary (0D)
Something could be called "atomic", but this old word lies. Atoms are not atomic, this is 100-year old paradox.
Elementary entities are minimal entities possible.
Minimal data chunk
Minimal delta or patch
Each meta-type (Aspect) defines shape of its minimal element as "Quantum" where Elementary Data ~ Quantum and Elementary Patch is std::optional<Quantum>

### Linear (1D)
It is homogenous 1-D set of Elementary objects
Linear Patch is simple container of Patclets
Linear State is container of data ~ [ Quantum ]

### Complex
Comlex data is heterogenous set of Lines
Complex Patch is set of Linear Patches, e.t.c


---

## Shapes
Evetu chape can be Elementary/Linear/Complex

### State
Something can be materialized, read, written

### Reality
Reality is State, which is already materialized

### Patch
A patch is a collection of bosons.
A patch describes a set of changes applicable to a state.
Patches are transition structures.
Patches are not themselves persistent state. Other side, Patch is best candidate for intermodel communication.

### Draft
Draft is State, which has hidden "current" Reality and Patch against it.
Reads from Draft show "State which should be after Patch be applied"
Writes to the Draft do not mutate State - Patch is affected instead.

### Delta
A delta describes the observable difference between states. Physically, Delta is {state, patch} view
A delta is derived information.
A delta may be computed from states, patches, bosons, or other transition structures.

---

## Type Axis

The type axis is the conceptual dimension separating lines inside a complex model.
A vector of type identifiers may be interpreted as movement along the type axis.
The type axis does not exist in linear models.

---

## Integration

Integration is the process of transforming state A into state A'.
During integration transient structures may exist temporarily.
Only the resulting state becomes part of persistent realm state.

---

## Model

The model layer describes forms, states, transitions, and compositions.
It defines the language used to describe realm structure independently of behavior, execution, processing, or feature logic.