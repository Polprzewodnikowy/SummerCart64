---
title: Game flow
order: 5
---

# Game flow specification

In the original Tomb Raider 1, the game flow was completely hard-coded,
including the levels, limiting the builders' flexibility. Tomb Raider 2
improved upon this by introducing a tombpc.dat binary file for game
configuration, although it remained cryptic and required builders to use
dedicated tooling. TRX has transitioned away from these earlier methods,
choosing to manage game flow with a JSON file that provides a unified
structure for both games. This document details the elements that can be
modified using this updated format.
