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

## State

A state is a persistent arrangement of particles within a realm.
States are stable and serializable.
Realm evolution transforms one state into another.

---

## Delta

A delta describes the observable difference between states. Physically, Delta is {state, patch} view
A delta is derived information.
A delta may be computed from states, patches, bosons, or other transition structures.

---

## Line

A line is a linear collection of homogeneous particles.
All particles within a line share the same quantum type.
A line is indexed by object identifiers.

Examples:

- Body line
- Physics line
- Mind line

Lines form the fundamental one-dimensional structures of the model.

---

## Linear Model

## A linear model contains a single line.
Mathematically it behaves as a one-dimensional structure.
The primary coordinate is object identity.

## Complex Model

A complex model contains multiple lines.
Mathematically it behaves as a two-dimensional structure.
The first coordinate is object identity.
The second coordinate is line type.
This interpretation is conceptually similar to extending a real axis with an orthogonal imaginary axis.
The analogy is structural rather than mathematical.

---

## Type Axis

The type axis is the conceptual dimension separating lines inside a complex model.
A vector of type identifiers may be interpreted as movement along the type axis.
The type axis does not exist in linear models.

---

## Future

A preview presents a future state without allowing modification.
A preview combines:

- state,
- transition information.

The result behaves as a read-only view of the projected state.

---

## Draft

A draft presents a future state while allowing modification.
A draft combines:

- state,
- mutable transition information.

Operational changes applied to a draft are accumulated rather than immediately committed.

---

## Patch

A patch is a collection of bosons.
A patch describes a set of changes applicable to a state.
Patches are transition structures.
Patches are not themselves persistent state. Other side, Patch is best candidate for intermodel communication.

---

## Integration

Integration is the process of transforming state A into state A'.
During integration transient structures may exist temporarily.
Only the resulting state becomes part of persistent realm state.

---

## Model

The model layer describes forms, states, transitions, and compositions.
It defines the language used to describe realm structure independently of behavior, execution, processing, or feature logic.