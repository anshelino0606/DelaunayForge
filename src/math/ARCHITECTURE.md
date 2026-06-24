# math architecture

```mermaid
flowchart LR
    C[PDEComponent] --> R[SolveRequest]
    R --> D[fem::solve]
    D --> F[FEMProblem adapter]
    F --> B[fem_discretization_dispatch]
    B --> P1[P1 assembly/solve]
```

```mermaid
flowchart TD
    P[PDEModel] --> R[SolveRequest]
    O[OperatorSpec] --> R
    G[BoundaryModel] --> R
    T[TimeStepState] --> R
    K[DiscretizationSpec] --> R
```

```mermaid
flowchart TD
    P1[P1 dispatcher] --> L[LocalEllipticSpec]
    P1 --> I[FractionalIntegralSpec]
    P1 --> G[FractionalRegionalSpec]
    P1 --> S[FractionalSpectralSpec]

    L --> A1[consistent local stiffness + mass + RHS]
    I --> A2[nonlocal pair matrix + exterior tail + consistent RHS]
    G --> A3[nonlocal pair matrix + consistent RHS]
    S --> A4[spectral modal solve]
```

```mermaid
flowchart LR
    M[FEMMesh] --> DM[DirichletMask]
    BM[BoundaryModel] --> DM
    BM --> BL[BoundaryLoadModel]
    DM --> E[Dirichlet elimination]
    BL --> SYS[FEMSystem]
```

```mermaid
flowchart LR
    types[types.h] --> Real
    types --> Index
    types --> Count
    Index --> Mesh[FEMMesh connectivity]
    Index --> CRS[CRS row/column indices]
    Count --> Sizes[mode counts / dof counts]
```

## Header / source split

```mermaid
flowchart LR
  H[.h declarations] --> CC[.cc implementation]
  H --> T[templates / constexpr only]
  CC --> S[compiled implementation]
```

Policy: non-template FEM/PDE implementation lives in `.cc`; headers keep type declarations, function declarations, lightweight templates, constexpr constants, and engine registration declarations.
