# DAQL Workspace

**DAQL** (Domain Agnostic Quantization Language) is an experimental workspace dedicated to the description, execution and exploration of quantized world models.

The project combines a domain-agnostic specification language, execution mechanisms and practical experiments built on top of them.

The long-term goal of DAQL is to provide a unified approach for describing structured state, defining its behavior and executing resulting world models in C++.

---

## Components

### Q1

Q1 is a specification language used to describe world models.

A Q1 document defines the structure of a domain, its types, relationships, actions, reactions and other elements required to construct an executable model.

Q1 serves as the primary authoring language of the workspace.

---

### FQSM

FQSM (Flat Quantized State Mechanism) is the execution mechanism used by generated models.

It provides the runtime infrastructure required to store, access, modify and synchronize quantized state.

FQSM defines the operational layer on which Q1-generated systems are executed.

---

### Raidenmamare
Small visualization mechanism for current research. Written on Q1 language and animated with fQSM runtime.
Literally, it is fat OpenGL tutorial "your first triangle" developed a bit.

---

### Aeris

Aeris is the primary application project developed within the DAQL workspace.

The project serves both as a practical target and as a long-term validation environment for DAQL and FQSM concepts.

Additional documentation will be provided separately.

---

## Philosophy

DAQL is based on a simple separation of responsibilities:

* Q1 describes a world.
* FQSM executes a world.
* Applications define concrete domains and behaviors.

This separation allows structural models, behavioral models and execution mechanisms to evolve independently while remaining compatible through a common language and execution layer.

---

## Status

DAQL is an active research and development project.

Concepts, APIs, naming and implementation details are expected to evolve as the system matures.
