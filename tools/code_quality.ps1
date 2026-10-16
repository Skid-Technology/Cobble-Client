


param(
    [switch]$Format,
    [switch]$Lint,
    [switch]$DeadCode,
    [switch]$OrphanFiles,
    [switch]$DependencyGraph,
    [switch]$All,
    [switch]$DryRun,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$RepoRoot = Split-Path -Parent $PSScriptRoot


$SourceDirs = @(
    "$RepoRoot\src",
    "$RepoRoot\loader\src"
)


$DynamicPrefixes = @(
    "JNI_",
    "Java_",
    "JNIEXPORT",
    "Hook_",
    "Callback_",
    "On",           
    "nativeOn"      
)


$IgnoredPatterns = @(
    "*_test.cpp",
    "*_legacy.cpp",
    "*_deprecated.cpp"
)


$ExcludeFromFormat = @(
    "inter_bold.hpp",      
    "inter_regular.hpp",   
    "bytes.hpp",           
    "tweetnacl.h",         
    "xor.h",               
    "icons.hpp",           
    "stb_*.h",             
    "*_gen.hpp"            
)

function Write-Step($msg) {
    Write-Host "`n[ QUALITY ] " -ForegroundColor Cyan -NoNewline
    Write-Host $msg
}

function Write-OK($msg) {
    Write-Host "  [OK] " -ForegroundColor Green -NoNewline
    Write-Host $msg
}

function Write-Warn($msg) {
    Write-Host "  [!] " -ForegroundColor Yellow -NoNewline
    Write-Host $msg
}

function Write-Item($msg) {
    Write-Host "      " -NoNewline
    Write-Host $msg -ForegroundColor Gray
}




function Invoke-Format {
    Write-Step "Formatting source files with clang-format..."
    
    $clangFormat = Get-Command "clang-format" -ErrorAction SilentlyContinue
    if (-not $clangFormat) {
        Write-Warn "clang-format not found! Install LLVM or add to PATH."
        return
    }
    
    $files = @()
    foreach ($dir in $SourceDirs) {
        if (Test-Path $dir) {
            $files += Get-ChildItem -Path $dir -Recurse -Include "*.cpp","*.hpp","*.h" -File
        }
    }
    
    $count = 0
    $skipped = 0
    foreach ($file in $files) {
        
        $shouldExclude = $false
        foreach ($pattern in $ExcludeFromFormat) {
            if ($file.Name -like $pattern) {
                $shouldExclude = $true
                $skipped++
                if ($Verbose) { Write-Item "Skipped (excluded): $($file.Name)" }
                break
            }
        }
        if ($shouldExclude) { continue }
        
        if ($DryRun) {
            $diff = & clang-format --dry-run --Werror $file.FullName 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Item "Would format: $($file.Name)"
                $count++
            }
        } else {
            & clang-format -i $file.FullName
            if ($Verbose) { Write-Item "Formatted: $($file.Name)" }
            $count++
        }
    }
    
    if ($DryRun) {
        Write-OK "$count files need formatting (skipped $skipped excluded files)"
    } else {
        Write-OK "Formatted $count files (skipped $skipped excluded files)"
    }
}




function Invoke-Lint {
    Write-Step "Running clang-tidy analysis..."
    
    $clangTidy = Get-Command "clang-tidy" -ErrorAction SilentlyContinue
    if (-not $clangTidy) {
        Write-Warn "clang-tidy not found! Install LLVM or add to PATH."
        return
    }
    
    
    $files = @()
    foreach ($dir in $SourceDirs) {
        if (Test-Path $dir) {
            $files += Get-ChildItem -Path $dir -Recurse -Include "*.cpp" -File
        }
    }
    
    $issues = 0
    foreach ($file in $files) {
        Write-Host "." -NoNewline
        $output = & clang-tidy $file.FullName -- -std=c++17 -I"$RepoRoot\src" 2>&1
        $fileIssues = ($output | Select-String "warning:|error:" | Measure-Object).Count
        if ($fileIssues -gt 0) {
            Write-Host ""
            Write-Warn "$($file.Name): $fileIssues issues"
            if ($Verbose) {
                $output | ForEach-Object { Write-Item $_ }
            }
            $issues += $fileIssues
        }
    }
    
    Write-Host ""
    if ($issues -eq 0) {
        Write-OK "No issues found!"
    } else {
        Write-Warn "Total: $issues issues found"
    }
}




function Invoke-DeadCode {
    Write-Step "Scanning for potentially dead code..."
    Write-Host "  (JNI/Hook functions are whitelisted)" -ForegroundColor DarkGray
    
    $definitions = @{}
    $references = @{}
    
    
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) { continue }
        
        $files = Get-ChildItem -Path $dir -Recurse -Include "*.cpp","*.hpp","*.h" -File
        foreach ($file in $files) {
            $content = Get-Content $file.FullName -Raw
            
            
            
            $defMatches = [regex]::Matches($content, '(?<!if|for|while|switch|return|case)\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^;]*\)\s*\{')
            foreach ($match in $defMatches) {
                $funcName = $match.Groups[1].Value
                if (-not $definitions.ContainsKey($funcName)) {
                    $definitions[$funcName] = $file.Name
                }
            }
            
            
            $refMatches = [regex]::Matches($content, '\b([a-zA-Z_][a-zA-Z0-9_]*)\b')
            foreach ($match in $refMatches) {
                $funcName = $match.Groups[1].Value
                if (-not $references.ContainsKey($funcName)) {
                    $references[$funcName] = 0
                }
                $references[$funcName]++
            }
        }
    }
    
    
    $deadCandidates = @()
    foreach ($func in $definitions.Keys) {
        
        $skip = $false
        foreach ($prefix in $DynamicPrefixes) {
            if ($func.StartsWith($prefix) -or $func -cmatch "^[a-z]") {
                
            }
            if ($func.StartsWith($prefix)) {
                $skip = $true
                break
            }
        }
        if ($skip) { continue }
        
        
        $refCount = $references[$func]
        if ($refCount -le 2) {  
            $deadCandidates += [PSCustomObject]@{
                Function = $func
                File = $definitions[$func]
                References = $refCount
            }
        }
    }
    
    if ($deadCandidates.Count -eq 0) {
        Write-OK "No obvious dead code found"
    } else {
        Write-Warn "Found $($deadCandidates.Count) potentially unused functions:"
        foreach ($item in $deadCandidates | Sort-Object File) {
            Write-Item "$($item.Function) in $($item.File) (refs: $($item.References))"
        }
        Write-Host ""
        Write-Host "  Note: Review manually. JNI/Hook functions may be false positives." -ForegroundColor DarkGray
    }
}




function Invoke-OrphanFiles {
    Write-Step "Checking for orphan source files..."
    
    $vcxprojFiles = @(
        "$RepoRoot\Cobble.vcxproj",
        "$RepoRoot\loader\CobbleLoader.vcxproj"
    )
    
    
    $listedFiles = @{}
    foreach ($proj in $vcxprojFiles) {
        if (-not (Test-Path $proj)) { continue }
        
        $content = Get-Content $proj -Raw
        $matches = [regex]::Matches($content, 'ClCompile\s+Include="([^"]+)"')
        foreach ($match in $matches) {
            $fileName = Split-Path $match.Groups[1].Value -Leaf
            $listedFiles[$fileName] = $true
        }
    }
    
    
    $orphans = @()
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) { continue }
        
        $files = Get-ChildItem -Path $dir -Recurse -Include "*.cpp" -File
        foreach ($file in $files) {
            
            $ignored = $false
            foreach ($pattern in $IgnoredPatterns) {
                if ($file.Name -like $pattern) {
                    $ignored = $true
                    break
                }
            }
            if ($ignored) { continue }
            
            if (-not $listedFiles.ContainsKey($file.Name)) {
                $orphans += $file
            }
        }
    }
    
    if ($orphans.Count -eq 0) {
        Write-OK "All source files are in .vcxproj"
    } else {
        Write-Warn "Found $($orphans.Count) orphan files (not in .vcxproj):"
        foreach ($file in $orphans) {
            Write-Item $file.FullName.Replace($RepoRoot, ".")
        }
        Write-Host ""
        Write-Host "  These files are NOT compiled. Delete or add to project." -ForegroundColor DarkGray
    }
}




function Invoke-DependencyGraph {
    Write-Step "Generating dependency graph..."
    
    $outputFile = "$RepoRoot\out\includes.dot"
    $edges = @()
    
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) { continue }
        
        $files = Get-ChildItem -Path $dir -Recurse -Include "*.cpp","*.hpp","*.h" -File
        foreach ($file in $files) {
            $content = Get-Content $file.FullName
            $from = $file.Name
            
            foreach ($line in $content) {
                if ($line -match '#include\s*[<"]([^>"]+)[>"]') {
                    $to = Split-Path $matches[1] -Leaf
                    $edges += "  `"$from`" -> `"$to`";"
                }
            }
        }
    }
    
    
    $dotContent = @"
digraph Includes {
    rankdir=LR;
    node [shape=box, fontsize=10];
$($edges -join "`n")
}
"@
    
    $outDir = Split-Path $outputFile -Parent
    if (-not (Test-Path $outDir)) {
        New-Item -ItemType Directory -Path $outDir -Force | Out-Null
    }
    
    $dotContent | Out-File $outputFile -Encoding UTF8
    Write-OK "Generated: $outputFile"
    Write-Host "  View with: dot -Tpng $outputFile -o includes.png" -ForegroundColor DarkGray
}





Write-Host "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•" -ForegroundColor DarkGray
Write-Host " Cobble Code Quality Tools" -ForegroundColor White
Write-Host "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•" -ForegroundColor DarkGray

if ($All) {
    $Format = $true
    $Lint = $true
    $DeadCode = $true
    $OrphanFiles = $true
}

if (-not ($Format -or $Lint -or $DeadCode -or $OrphanFiles -or $DependencyGraph)) {
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  .\tools\code_quality.ps1 -Format        # Apply clang-format"
    Write-Host "  .\tools\code_quality.ps1 -Lint          # Run clang-tidy"
    Write-Host "  .\tools\code_quality.ps1 -DeadCode      # Find unused functions"
    Write-Host "  .\tools\code_quality.ps1 -OrphanFiles   # Find files not in .vcxproj"
    Write-Host "  .\tools\code_quality.ps1 -DependencyGraph  # Generate include graph"
    Write-Host "  .\tools\code_quality.ps1 -All           # Run all checks"
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -DryRun   # Preview changes without applying"
    Write-Host "  -Verbose  # Show detailed output"
    exit 0
}

if ($Format) { Invoke-Format }
if ($Lint) { Invoke-Lint }
if ($DeadCode) { Invoke-DeadCode }
if ($OrphanFiles) { Invoke-OrphanFiles }
if ($DependencyGraph) { Invoke-DependencyGraph }

Write-Host "`nâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•" -ForegroundColor DarkGray
Write-Host " Done!" -ForegroundColor Green
Write-Host "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•" -ForegroundColor DarkGray
