# Math architecture

```mermaid
flowchart LR
    UI[PDEComponent] --> Req[build SolveRequest]
    Req --> PDE[pde/
model + operator + boundary + request]
    PDE --> FEM[fem/
solve(request, mesh)]
    FEM --> Assembly[P1 assembly]
    Assembly --> Ops[operators/
reusable kernels]
    Assembly --> Solver[CG / dense / spectral]
    Solver --> Sol[solution]
```

```mermaid
flowchart TD
    SolveRequest --> PDEModel
    SolveRequest --> OperatorSpec
    SolveRequest --> BoundaryModel
    SolveRequest --> DiscretizationSpec
    OperatorSpec --> LocalEllipticSpec
    OperatorSpec --> FractionalIntegralSpec
    OperatorSpec --> FractionalRegionalSpec
    OperatorSpec --> FractionalSpectralSpec
```

```mermaid
flowchart TD
    OperatorSpec --> Dispatch{fem::solve_fem}
    Dispatch --> Local[local sparse P1]
    Dispatch --> Integral[fractional integral P1]
    Dispatch --> Regional[fractional regional P1]
    Dispatch --> Spectral[spectral modal path]
    Integral -. large N .-> MatrixFree[FractionalMatrixFreeP1Operator]
    Regional -. large N .-> MatrixFree
```

```mermaid
flowchart TD
    BoundaryModel --> Mask[DirichletMask]
    Mask --> Assembly[emit diagonal during assembly]
    Mask --> Rebuild[legacy CRS rebuild elimination]
    BoundaryModel --> Natural[BoundaryLoadModel]
    Natural --> Neumann
    Natural --> Robin
```

```mermaid
flowchart LR
    types[types.h
Real, Index:uint32_t, Count:uint32_t] --> pde
    types --> fem
    types --> operators
```
