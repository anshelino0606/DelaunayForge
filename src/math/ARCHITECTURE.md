# PDE / FEM / Fractional Architecture

## Constraints

| Rule | Keep |
|---|---|
| Public notation | `assemble_poisson_P1`, `assemble_fractional_laplacian_P1`, `assemble_and_solve_*_P1`, `PDEPreset::apply`, `PDEPreset::fem_assembler`, `FEMProblem`, `DifferentialEquation` |
| Mathematical split | PDE model ≠ operator definition ≠ discretization ≠ solver |
| Fractional split | `Integral` ≠ `Regional` ≠ `Spectral` |
| Migration | add adapters, do not break callers |

---

## Current shape

```mermaid
flowchart TD
    PDEComponent["PDEComponent::solve"] --> Preset["PDEPreset"]
    Preset -->|"apply(FEMProblem&)"| FEMProblem["FEMProblem"]
    Preset -->|"fem_assembler()"| FEMAssembler["FEMAssembler fn ptr"]

    FEMProblem -->|"inherits"| DifferentialEquation["DifferentialEquation"]
    DifferentialEquation --> Coeffs["a, c, f, time"]
    DifferentialEquation --> FractionalCfg["optional<FractionalEquationConfig>"]

    FEMProblem --> Mesh["FEMMesh*"]
    FEMProblem --> TimeState["dt, u_prev"]

    FEMAssembler --> P1Dispatch["assemble_and_solve_*_P1"]
    P1Dispatch --> Local["assemble_poisson_P1"]
    P1Dispatch --> DenseFrac["assemble_fractional_laplacian_P1"]
    P1Dispatch --> Spectral["assemble_and_solve_spectral_fractional_P1"]

    DenseFrac --> IntegralAndRegional["Integral + Regional currently share dense path"]

    classDef risk fill:#ffecec,stroke:#c00,stroke-width:2px,color:#111;
    class DifferentialEquation,FEMProblem,Preset,IntegralAndRegional risk;
```

---

## Current coupling map

```mermaid
flowchart LR
    A["PDEParameter"] -->|"mutates"| B["DifferentialEquation"]
    B -->|"owns"| C["FractionalEquationConfig"]
    D["FEMProblem"] -->|"is-a"| B
    E["PDEPreset"] -->|"chooses"| F["FEM assembler"]
    F -->|"switches on"| C
    C -->|"type flag"| G["Integral / Regional / Spectral"]

    classDef mixed fill:#fff3cd,stroke:#b58900,color:#111;
    class B,C,D,E,F mixed;
```

---

## Target layer boundary

```mermaid
flowchart TD
    UI["PDEComponent / UI / Presets"] --> RequestFactory["SolveRequestFactory"]

    RequestFactory --> Model["PDEModel"]
    RequestFactory --> Operator["OperatorSpec"]
    RequestFactory --> Discretization["DiscretizationSpec"]
    RequestFactory --> Time["TimeState"]

    Model --> ModelData["a, c, f, time, boundary"]
    Operator --> LocalOp["LocalEllipticSpec"]
    Operator --> IntegralOp["FractionalIntegralSpec"]
    Operator --> RegionalOp["FractionalRegionalSpec"]
    Operator --> SpectralOp["FractionalSpectralSpec"]

    Discretization --> FEM["FEM / P1 / mesh / quadrature"]
    Discretization --> Future["BEM / FDM / meshless / spectral grid"]

    RequestFactory --> Dispatcher["SolverDispatcher"]
    Dispatcher --> FEMBackend["FEMBackend<P1>"]
    Dispatcher --> FutureBackend["other backend"]

    FEMBackend --> LocalAsm["LocalP1Assembler"]
    FEMBackend --> IntegralAsm["FractionalIntegralP1Assembler"]
    FEMBackend --> RegionalAsm["FractionalRegionalP1Assembler"]
    FEMBackend --> SpectralSolve["FractionalSpectralP1Solver"]

    classDef math fill:#e8f4ff,stroke:#1677ff,color:#111;
    classDef backend fill:#f0fff4,stroke:#239a3b,color:#111;
    classDef future fill:#f7f7f7,stroke:#888,color:#111;
    class Model,Operator,LocalOp,IntegralOp,RegionalOp,SpectralOp math;
    class FEMBackend,LocalAsm,IntegralAsm,RegionalAsm,SpectralSolve backend;
    class Future,FutureBackend future;
```

---

## Target types

```mermaid
classDiagram
    class PDEModel {
        +Coefficient a
        +Coefficient c
        +Coefficient f
        +double time
        +BoundaryModel boundary
    }

    class OperatorSpec {
        <<variant>>
    }

    class LocalEllipticSpec
    class FractionalIntegralSpec {
        +double s
        +double scale
    }
    class FractionalRegionalSpec {
        +double s
        +double scale
    }
    class FractionalSpectralSpec {
        +double s
        +double scale
        +double eig_clip
        +int spectral_k
    }

    class DiscretizationSpec {
        +Backend backend
        +Basis basis
        +Quadrature quadrature
        +BCPolicy bc_policy
    }

    class TimeState {
        +double dt
        +span u_prev
        +span v_prev
        +span a_prev
    }

    class SolveRequest {
        +PDEModel model
        +OperatorSpec op
        +DiscretizationSpec discretization
        +TimeState time
    }

    PDEModel --> SolveRequest
    OperatorSpec --> SolveRequest
    DiscretizationSpec --> SolveRequest
    TimeState --> SolveRequest

    OperatorSpec <|.. LocalEllipticSpec
    OperatorSpec <|.. FractionalIntegralSpec
    OperatorSpec <|.. FractionalRegionalSpec
    OperatorSpec <|.. FractionalSpectralSpec
```

---

## Keep notation by using facades

```mermaid
flowchart TD
    subgraph PublicAPI["public notation stays"]
        A1["assemble_poisson_P1(P)"]
        A2["assemble_fractional_laplacian_P1(P, s, C_scale)"]
        A3["assemble_and_solve_fractional_auto_P1(P, out)"]
        A4["PDEPreset::apply(equation)"]
        A5["PDEPreset::fem_assembler()"]
    end

    subgraph NewCore["new internal core"]
        R["SolveRequest"]
        D["SolverDispatcher"]
        O["Operator-specific implementation"]
    end

    A1 --> R
    A2 --> R
    A3 --> R
    A4 --> R
    A5 --> D
    R --> D --> O
```

---

## Operator routing

```mermaid
flowchart LR
    Request["SolveRequest"] --> Op{"OperatorSpec"}

    Op -->|"LocalEllipticSpec"| Local["assemble_poisson_P1 facade"]
    Op -->|"FractionalIntegralSpec"| Integral["assemble_fractional_integral_P1 core"]
    Op -->|"FractionalRegionalSpec"| Regional["assemble_fractional_regional_P1 core"]
    Op -->|"FractionalSpectralSpec"| Spectral["assemble_and_solve_spectral_fractional_P1 facade"]

    Local --> Sparse["CRS + CG"]
    Integral --> DenseKernel["dense nonlocal kernel"]
    Regional --> DenseRegional["dense regional kernel"]
    Spectral --> Eigen["K φ = λ M φ"]

    classDef strict fill:#ffecec,stroke:#c00,color:#111;
    class Regional strict;
```

---

## Fractional operator semantics

| `FractionalType` | Mathematical object | Backend path | Boundary meaning | Rule |
|---|---|---|---|---|
| `Integral` | restricted / Riesz-type nonlocal operator | dense kernel | exterior interaction / killed extension | separate core |
| `Regional` | censored operator on `Ω` | dense regional kernel | integration restricted to `Ω` | never silently alias to `Integral` |
| `Spectral` | fractional power of local operator | eigenbasis of local FEM operator | inherited from local BCs | separate solver |

---

## FEM backend decomposition

```mermaid
flowchart TD
    FEMBackend["FEMBackend<P1>"] --> BuildSpace["build_space(mesh, P1)"]
    FEMBackend --> BuildForms["build_forms(model, op)"]
    FEMBackend --> ApplyBC["apply_boundary_policy"]
    FEMBackend --> Solve["solve"]
    FEMBackend --> Pack["fill_solution"]

    BuildForms --> LocalForm["Local bilinear / RHS"]
    BuildForms --> IntegralForm["Integral nonlocal form"]
    BuildForms --> RegionalForm["Regional nonlocal form"]
    BuildForms --> SpectralForm["Spectral modal form"]

    ApplyBC --> StrongD["Dirichlet elimination"]
    ApplyBC --> WeakN["Neumann load"]
    ApplyBC --> Robin["Robin boundary mass + load"]
```

---

## Correct solve sequence

```mermaid
sequenceDiagram
    participant C as PDEComponent
    participant P as PDEPreset
    participant F as SolveRequestFactory
    participant D as SolverDispatcher
    participant B as FEMBackend<P1>
    participant S as Solver

    C->>P: selected preset + parameters
    P->>F: apply existing parameters
    F->>F: build PDEModel + OperatorSpec + DiscretizationSpec
    F->>D: SolveRequest
    D->>B: backend = FEM, basis = P1
    B->>B: assemble operator-specific system
    B->>B: apply BC policy
    B->>S: solve system / modal system
    S-->>B: x
    B-->>C: DifferentialEquationSolution
```

---

## Boundary-condition ownership

```mermaid
flowchart TD
    BoundaryModel["BoundaryModel"] --> Dirichlet["Dirichlet"]
    BoundaryModel --> Neumann["Neumann"]
    BoundaryModel --> Robin["Robin"]

    Dirichlet --> LocalPolicy["local: strong elimination"]
    Dirichlet --> IntegralPolicy["integral: exterior/volume policy"]
    Dirichlet --> RegionalPolicy["regional: no exterior killing"]
    Dirichlet --> SpectralPolicy["spectral: eigenproblem domain restriction"]

    Neumann --> LocalBoundaryLoad["local boundary load"]
    Neumann --> SpectralEigenBC["spectral local eigen-BC"]

    Robin --> LocalRobin["local boundary mass + load"]
    Robin --> SpectralRobin["spectral local eigen-BC"]

    classDef warning fill:#fff3cd,stroke:#b58900,color:#111;
    class IntegralPolicy,RegionalPolicy warning;
```

---

## Migration plan

```mermaid
gantt
    title Backward-compatible migration
    dateFormat  YYYY-MM-DD
    axisFormat  %d.%m

    section Phase 0
    Document architecture                         :done, p0, 2026-06-22, 1d
    Remove duplicate stationary assembler call    :crit, p0b, after p0, 1d

    section Phase 1
    Add PDEModel / OperatorSpec / SolveRequest    :p1, after p0b, 2d
    Add adapters from DifferentialEquation        :p1b, after p1, 1d

    section Phase 2
    Move dispatch out of fem_assemblers_p1.h      :p2, after p1b, 2d
    Split Integral and Regional implementations   :crit, p2b, after p2, 3d

    section Phase 3
    Deprecate DifferentialEquation::fractional as source of truth :p3, after p2b, 2d
    Add non-FEM backend slot                       :p3b, after p3, 2d
```

---

## Minimal file layout

```mermaid
flowchart TD
    math["math/"] --> model["pde/model/"]
    math --> operator["pde/operator/"]
    math --> request["pde/solve_request.h"]
    math --> femdir["fem/"]

    model --> PDEModelH["pde_model.h"]
    model --> BoundaryModelH["boundary_model.h"]

    operator --> OperatorSpecH["operator_spec.h"]
    operator --> LocalSpecH["local_elliptic_spec.h"]
    operator --> FractionalSpecH["fractional_operator_spec.h"]

    femdir --> adapter["fem_problem_adapter.h"]
    femdir --> backend["fem_backend_p1.h"]
    femdir --> opasm["operators/"]

    opasm --> localp1["local_p1_assembler.h"]
    opasm --> integralp1["fractional_integral_p1_assembler.h"]
    opasm --> regionalp1["fractional_regional_p1_assembler.h"]
    opasm --> spectralp1["fractional_spectral_p1_solver.h"]
```

---

## Compatibility map

| Existing symbol | Status | New role |
|---|---|---|
| `DifferentialEquation` | keep | legacy PDE coefficient carrier; adapter source |
| `DifferentialEquation::fractional` | keep temporarily | legacy mirror of `OperatorSpec` |
| `FEMProblem` | keep | FEM bridge/request view |
| `PDEPreset::apply` | keep | fills legacy parameters and/or request factory |
| `PDEPreset::fem_assembler` | keep | facade to dispatcher |
| `assemble_fractional_laplacian_P1` | keep | compatibility wrapper; route only to `Integral` unless explicitly named otherwise |
| `assemble_and_solve_fractional_auto_P1` | keep | dispatches by `OperatorSpec` |
| `FractionalIntegralOperator` | keep as alias | compatibility name for `FractionalElementContribution` |

---

## Immediate corrections

```mermaid
flowchart TD
    X1["PDEComponent::solve stationary branch"] --> X2["remove duplicate assembler(fem_problem, cached_sol.solution)"]
    Y1["assemble_and_solve_fractional_auto_P1"] --> Y2["Regional must not fall through to Integral"]
    Z1["DifferentialEquation::fractional"] --> Z2["move to OperatorSpec source-of-truth"]
    W1["PDEPreset"] --> W2["stop owning backend choice long-term"]

    classDef crit fill:#ffecec,stroke:#c00,stroke-width:2px,color:#111;
    class X2,Y2 crit;
```

---

## Mathematical invariant checklist

```mermaid
flowchart TD
    A["Before solve"] --> B{"operator chosen?"}
    B -->|"no"| Local["LocalEllipticSpec"]
    B -->|"yes"| C{"type"}

    C --> Integral["Integral: restricted/Riesz kernel"]
    C --> Regional["Regional: censored Ω-kernel"]
    C --> Spectral["Spectral: local eigenbasis"]

    Integral --> ICheck{"exterior/boundary policy explicit?"}
    Regional --> RCheck{"no silent Integral alias?"}
    Spectral --> SCheck{"BCs included in local eigenproblem?"}

    ICheck --> OK["assemble"]
    RCheck --> OK
    SCheck --> OK
```

---

## Final target

```mermaid
flowchart LR
    Preset["PDEPreset"] --> Math["PDEModel + OperatorSpec"]
    Math --> Request["SolveRequest"]
    Request --> Dispatch["SolverDispatcher"]
    Dispatch --> FEM["FEMBackend<P1>"]
    Dispatch --> Other["future backend"]
    FEM --> System["FEMSystem"]
    System --> Solution["DifferentialEquationSolution"]

    Legacy["existing functions"] --> Facades["facades"] --> Request
```
