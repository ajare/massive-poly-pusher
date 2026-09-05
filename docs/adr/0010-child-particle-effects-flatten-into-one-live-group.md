---
status: accepted
---

# Child particle effects flatten into one live group

A particle effect may reference child particle effect assets. At asset creation,
the CPU recursively flattens every referenced emitter template into the parent
in authored depth-first order. A child transform is composed outside each
existing descendant transform, and its seed salt is deterministically mixed
into every descendant emitter seed. Equal source seeds and equal salts reproduce
the same stream deliberately; authors use different salts for independent
copies. Child-reference cycles are rejected.

The resulting templates form one live particle effect. Transform, visibility,
destruction, and generational lifetime therefore apply to the whole group, and
the group remains alive until its longest-lived emitter retires. Intra-child
secondary-particle-burst indices are rebased while flattening, but those GPU
events remain local to the copied child branch.

Budgets are not pooled. Each child asset validates its own effect-level maximum
against its local emitter templates, and runtime enforcement remains the existing
per-template clamp. A parent maximum likewise describes only its directly
authored emitter templates. Flattening preserves those independent limits and
avoids making the same child asset's behaviour depend on what else a parent
happens to include.

Keeping composition on the CPU avoids adding recursive asset concepts, hierarchy
walking, or group accounting to GPU simulation. Creating separate live effects
for children was rejected because parent transform, visibility, destruction, and
longest-child lifetime would then require a second hierarchy of runtime handles.
A shared aggregate budget was rejected because it would make spawn contention
order-dependent and weaken the reusable child asset's authored contract.
