---
title: Object
order: 14
---

## Object module

Module for controlling game objects.

### Functions

- [lua]`trx.objects.swap_mesh(obj1_id, obj2_id, mesh1_num, mesh2_num)`  
    Swaps the given meshes of the given objects.  
    Examples:
    - [lua]`trx.objects.swap_mesh(trx.catalog.objects.pierre, trx.catalog.objects.larson, 8, 8)`  
      Pierre now has Larson's head, and vice-versa

- [lua]`trx.objects.swap_mesh(obj1_id, obj2_id)`  
    Similar to above, but this will swap out all meshes rather than specific ones. This works best when both objects have the same mesh count; if one object has fewer meshes than the other, the minimum count will be used.  
    Examples:
    - [lua]`trx.objects.swap_mesh(trx.catalog.objects.pierre, trx.catalog.objects.larson)`  
      Pierre and Larson's meshes are fully swapped
    - [lua]`trx.objects.swap_mesh(trx.catalog.objects.pierre, trx.catalog.objects.warrior_1)`  
      Pierre's 15 meshes are now of mutant type; the mutant's first 15 meshes are Pierre's, the rest are default.
