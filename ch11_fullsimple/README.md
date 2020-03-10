# Typed Lamda Calculus (with varios extensions)

## Syntax

### Terms

```
t ::=
    x
    l x:T. t
    t t
    true
    false
    if t then t else t
    0
    succ t
    pred t
    iszero t
```

### Values

```
v ::=
    x
    l x:T. t
    true
    false
    0
    succ v
```

### Types

```
T ::=
    Bool
    Nat
    T -> T
```

### Contexts

```
Γ ::=
    Φ
    Γ, x:T
```

## Typing Rules

TODO

## Evaluation Rules

TODO