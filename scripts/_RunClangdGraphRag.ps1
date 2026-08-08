param(
    [string]$ApiKey,
    [string]$ApiKeyEnvName = "SI_API_KEY",
    [string]$ProjectRoot = "D:\PRJ\NoMoreDay",
    [string]$RagRoot = "F:\devtools\clangd-graph-rag",
    [string]$CondaEnv = "ai",
    [string]$SummaryModel = "Qwen/Qwen3-8B",
    [string]$EmbeddingModel = "Qwen/Qwen3-Embedding-0.6B",
    [string]$BaseUrl = "https://api.siliconflow.cn/v1",
    [string]$LlvmBin = "D:\Program Files\LLVM\bin",
    [string]$Neo4jUri = "bolt://localhost:7687",
    [string]$Neo4jUser = "neo4j",
    [string]$Neo4jPassword = "",
    [string]$Neo4jPasswordEnvName = "NEO4J_PASSWORD",
    [string]$Neo4jContainerName = "neo4j",
    [string]$Neo4jImage = "neo4j:5.26.21",
    [string]$Neo4jRoot = "F:\neo4j",
    [object]$StartNeo4j = $true,
    [object]$RunBuilder = $true,
    [object]$StartMcpServer = $true,
    [string]$McpTransport = "sse",
    [string]$McpHost = "0.0.0.0",
    [int]$McpPort = 8800,
    [string]$EmbeddingApi = "openai",
    [object]$Ingest = $true,
    [switch]$NoNeo4jStart,
    [switch]$NoBuilder,
    [switch]$NoMcpServer,
    [switch]$SkipSummary
)

$ErrorActionPreference = "Stop"
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $false
}

function Resolve-SecretValue {
    param(
        [string]$ExplicitValue,
        [string]$EnvName,
        [string]$MissingMessage
    )

    $value = $ExplicitValue
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = [Environment]::GetEnvironmentVariable($EnvName, "Process")
    }
    if ([string]::IsNullOrWhiteSpace($value)) {
        $value = [Environment]::GetEnvironmentVariable($EnvName, "User")
    }
    if ([string]::IsNullOrWhiteSpace($value)) {
        throw $MissingMessage
    }
    return $value
}

function Convert-ToBoolean {
    param(
        [object]$Value,
        [bool]$DefaultValue = $true,
        [string]$Name = "flag"
    )

    if ($null -eq $Value) {
        return $DefaultValue
    }

    if ($Value -is [bool]) {
        return [bool]$Value
    }

    $text = [string]$Value
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $DefaultValue
    }

    switch -Regex ($text.Trim().ToLowerInvariant()) {
        '^(1|true|t|yes|y|on)$' { return $true }
        '^(0|false|f|no|n|off)$' { return $false }
        default { throw "Invalid boolean for '$Name': $text" }
    }
}

function Test-DockerReady {
    try {
        cmd /c "docker info >nul 2>nul"
        return ($LASTEXITCODE -eq 0)
    } catch {
        return $false
    }
}

function Ensure-DockerReady {
    if (Test-DockerReady) {
        return
    }

    $dockerDesktopCandidates = @(
        "D:\Program Files\Docker\Docker\Docker Desktop.exe",
        "C:\Program Files\Docker\Docker\Docker Desktop.exe"
    )

    $dockerDesktop = $dockerDesktopCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $dockerDesktop) {
        throw "Docker daemon is not ready and Docker Desktop executable was not found."
    }

    Write-Host "[Run] Docker daemon not ready. Launching Docker Desktop..."
    Start-Process -FilePath $dockerDesktop | Out-Null

    for ($i = 0; $i -lt 180; $i++) {
        Start-Sleep -Seconds 1
        if (Test-DockerReady) {
            Write-Host "[Run] Docker daemon is ready."
            return
        }
    }

    throw "Docker daemon did not become ready in time."
}

function Repair-IndexKindLangIfNeeded {
    param([string]$IndexPath)

    if (-not (Test-Path $IndexPath)) {
        return $false
    }

    $lines = Get-Content -Path $IndexPath -Encoding utf8
    $fixed = New-Object System.Collections.Generic.List[string]
    $regex = '^(\s*)Kind:\s*(.*?)\s+Lang:\s*(\S.*?)\s*$'
    $fixCount = 0

    foreach ($line in $lines) {
        $match = [regex]::Match($line, $regex)
        if ($match.Success) {
            $indent = $match.Groups[1].Value
            $kind = $match.Groups[2].Value.Trim()
            $lang = $match.Groups[3].Value.Trim()
            $fixed.Add("${indent}Kind:            $kind")
            $fixed.Add("${indent}Lang:            $lang")
            $fixCount++
            continue
        }
        $fixed.Add($line)
    }

    if ($fixCount -gt 0) {
        [System.IO.File]::WriteAllText($IndexPath, ($fixed -join "`n") + "`n", [System.Text.UTF8Encoding]::new($false))
        Write-Host "[Run] Repaired malformed Kind/Lang lines in index.yaml: $fixCount"
        return $true
    }

    return $false
}

function Clear-StaleBuilderCaches {
    param([string]$ProjectRoot)

    $cacheDir = Join-Path $ProjectRoot ".cache"
    if (Test-Path $cacheDir) {
        Get-ChildItem -Path $cacheDir -Filter "parsing_*.pkl" -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    }
}

function Ensure-Neo4jRunning {
    param(
        [string]$ContainerName,
        [string]$Image,
        [string]$Root,
        [string]$User,
        [string]$Password
    )

    $dataDir = Join-Path $Root "data"
    $logsDir = Join-Path $Root "logs"
    $pluginsDir = Join-Path $Root "plugins"
    $importDir = Join-Path $Root "import"
    $envFile = Join-Path $Root "neo4j.env"

    New-Item -ItemType Directory -Force -Path $dataDir, $logsDir, $pluginsDir, $importDir | Out-Null

    @(
        "NEO4J_AUTH=$User/$Password",
        'NEO4J_PLUGINS=["apoc"]',
        "NEO4J_dbms_security_procedures_unrestricted=apoc.*",
        "NEO4J_dbms_security_procedures_allowlist=apoc.*"
    ) | Set-Content -Path $envFile -Encoding ascii

    $running = docker ps --filter "name=^/$ContainerName$" --format "{{.Names}}"
    if (-not [string]::IsNullOrWhiteSpace($running)) {
        Write-Host "[Neo4j] Container '$ContainerName' already running."
    } else {
        $exists = docker ps -a --filter "name=^/$ContainerName$" --format "{{.Names}}"
        if (-not [string]::IsNullOrWhiteSpace($exists)) {
            Write-Host "[Neo4j] Starting existing container '$ContainerName'..."
            docker start $ContainerName | Out-Null
        } else {
            Write-Host "[Neo4j] Creating container '$ContainerName' using image '$Image'..."
            docker run -d --name $ContainerName `
                -p 7474:7474 -p 7687:7687 `
                --env-file $envFile `
                -v "${dataDir}:/data" `
                -v "${logsDir}:/logs" `
                -v "${pluginsDir}:/plugins" `
                -v "${importDir}:/import" `
                $Image | Out-Null
        }
    }

    for ($i = 0; $i -lt 45; $i++) {
        Start-Sleep -Seconds 2
        $status = docker inspect $ContainerName --format "{{.State.Running}}" 2>$null
        if ($status -ne "true") {
            continue
        }

        $prevErrorAction = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & docker exec $ContainerName cypher-shell -u $User -p $Password "RETURN 1;" *> $null
            if ($LASTEXITCODE -eq 0) {
                Write-Host "[Neo4j] Ready."
                return
            }
        } catch {
            # Neo4j may still be initializing; retry.
        } finally {
            $ErrorActionPreference = $prevErrorAction
        }
    }

    throw "Neo4j did not become ready in time. Check: docker logs $ContainerName --tail 120"
}

$indexYaml = Join-Path $ProjectRoot "index.yaml"
$compileCommands = Join-Path $ProjectRoot "compile_commands.json"
$builderScript = Join-Path $RagRoot "clangd_graph_rag_builder.py"
$mcpServerScript = Join-Path $RagRoot "graph_mcp_server.py"
$libclangDll = Join-Path $LlvmBin "libclang.dll"

if (-not (Test-Path $builderScript)) {
    throw "Missing builder script: $builderScript"
}
if ($RunBuilder) {
    if (-not (Test-Path $indexYaml)) {
        throw "Missing index file: $indexYaml"
    }
    if (-not (Test-Path $compileCommands)) {
        throw "Missing compile_commands.json: $compileCommands"
    }
}
if ($StartMcpServer -and -not (Test-Path $mcpServerScript)) {
    throw "Missing MCP server script: $mcpServerScript"
}
if (-not (Test-Path $libclangDll)) {
    throw "Missing libclang.dll: $libclangDll"
}

$StartNeo4j = Convert-ToBoolean -Value $StartNeo4j -DefaultValue $true -Name "StartNeo4j"
$RunBuilder = Convert-ToBoolean -Value $RunBuilder -DefaultValue $true -Name "RunBuilder"
$StartMcpServer = Convert-ToBoolean -Value $StartMcpServer -DefaultValue $true -Name "StartMcpServer"
$Ingest = Convert-ToBoolean -Value $Ingest -DefaultValue $true -Name "Ingest"

if ($NoNeo4jStart) {
    $StartNeo4j = $false
}
if ($NoBuilder) {
    $RunBuilder = $false
}
if ($NoMcpServer) {
    $StartMcpServer = $false
}

$resolvedApiKey = Resolve-SecretValue -ExplicitValue $ApiKey -EnvName $ApiKeyEnvName -MissingMessage "Missing API key. Pass -ApiKey or define user env var '$ApiKeyEnvName'."
$resolvedNeo4jPassword = Resolve-SecretValue -ExplicitValue $Neo4jPassword -EnvName $Neo4jPasswordEnvName -MissingMessage "Missing Neo4j password. Pass -Neo4jPassword or define user env var '$Neo4jPasswordEnvName'."

if ($StartNeo4j) {
    Ensure-DockerReady
    Ensure-Neo4jRunning -ContainerName $Neo4jContainerName -Image $Neo4jImage -Root $Neo4jRoot -User $Neo4jUser -Password $resolvedNeo4jPassword
}

if ($RunBuilder) {
    $repairedIndex = Repair-IndexKindLangIfNeeded -IndexPath $indexYaml
    if ($repairedIndex) {
        $indexCache = Join-Path $ProjectRoot "index.pkl"
        if (Test-Path $indexCache) {
            Remove-Item -Force $indexCache
            Write-Host "[Run] Removed stale index cache: $indexCache"
        }
    }

    Clear-StaleBuilderCaches -ProjectRoot $ProjectRoot
}

$env:OPENAI_API_KEY = $resolvedApiKey
$env:OPENAI_BASE_URL = $BaseUrl
$env:OPENAI_MODEL = $SummaryModel
$env:OPENAI_EMBEDDING_MODEL = $EmbeddingModel
$env:EMBEDDING_API = $EmbeddingApi
$env:PROJECT_ROOT_PATH = $ProjectRoot
$env:LIBCLANG_PATH = $LlvmBin
$env:PATH = "$LlvmBin;$env:PATH"
$env:PYTHONUTF8 = "1"
$env:NEO4J_URI = $Neo4jUri
$env:NEO4J_USER = $Neo4jUser
$env:NEO4J_PASSWORD = $resolvedNeo4jPassword

$args = @(
    "run", "--no-capture-output", "-n", $CondaEnv,
    "python", $builderScript,
    $indexYaml,
    $ProjectRoot,
    "--llm-api", "openai",
    "--compile-commands", $compileCommands
)

if ($Ingest) {
    $args += "--ingest"
}
if (-not $SkipSummary) {
    $args += "--generate-summary"
}

Write-Host "[Run] ProjectRoot:     $ProjectRoot"
Write-Host "[Run] IndexYaml:       $indexYaml"
Write-Host "[Run] Conda env:       $CondaEnv"
Write-Host "[Run] LLM API:         openai-compatible"
Write-Host "[Run] Base URL:        $BaseUrl"
Write-Host "[Run] API key var:     $ApiKeyEnvName"
Write-Host "[Run] LLVM bin:        $LlvmBin"
Write-Host "[Run] Neo4j URI:       $Neo4jUri"
Write-Host "[Run] Neo4j user:      $Neo4jUser"
Write-Host "[Run] Neo4j pass var:  $Neo4jPasswordEnvName"
Write-Host "[Run] Start Neo4j:     $StartNeo4j"
Write-Host "[Run] Run builder:     $RunBuilder"
Write-Host "[Run] Ingest:          $Ingest"
Write-Host "[Run] Summary:         $([string](-not $SkipSummary))"
Write-Host "[Run] Summary model:   $SummaryModel"
Write-Host "[Run] Embedding model: $EmbeddingModel"
Write-Host "[Run] Embedding api:   $EmbeddingApi"
Write-Host "[Run] Start MCP:       $StartMcpServer"
Write-Host "[Run] MCP transport:   $McpTransport"
Write-Host "[Run] MCP host:port:   ${McpHost}:$McpPort"

if ($RunBuilder) {
    & conda @args
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($StartMcpServer) {
    $env:MCP_TRANSPORT = $McpTransport
    $env:MCP_HOST = $McpHost
    $env:MCP_PORT = [string]$McpPort
    $env:FASTMCP_HOST = $McpHost
    $env:FASTMCP_PORT = [string]$McpPort
    $env:FASTMCP_SERVER_HOST = $McpHost
    $env:FASTMCP_SERVER_PORT = [string]$McpPort

    Write-Host "[Run] Starting MCP server..."
    & conda run --no-capture-output -n $CondaEnv python $mcpServerScript
}
