$ErrorActionPreference = "Stop"

winget install `
    --id Microsoft.VisualStudio.2022.BuildTools `
    --exact `
    --accept-package-agreements `
    --accept-source-agreements `
    --silent `
    --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
