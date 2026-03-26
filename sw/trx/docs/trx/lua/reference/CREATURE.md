---
title: Creatures
order: 12
---

## Creatures module

Module for controlling certain creature behavior.

### Functions

- [lua]`trx.creatures.hostile_allies`  
    Reads/writes the global flag to indicate if allies are hostile towards Lara.  
    Examples:
    - [lua]`trx.creatures.hostile_allies = true`  
      All allies are now hostile

- [lua]`trx.creatures.add_ally(obj_id)`  
    Marks the given object as being an ally of Lara.  
    Examples:
    - [lua]`trx.creatures.add_ally(trx.catalog.objects.monk_1)`  
      All items of type `O_MONK_1` are now an ally of Lara

- [lua]`trx.creatures.add_ally_target(obj_id)`  
    Marks the given object as being one who will fight with any allies of Lara.  
    Examples:
    - [lua]`trx.creatures.add_ally_target(trx.catalog.objects.bandit_1)`  
      All items of type `O_BANDIT_1` will now target any allies of Lara.
