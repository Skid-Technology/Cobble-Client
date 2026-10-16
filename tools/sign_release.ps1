


param(
    [Parameter(Mandatory=$true)]
    [string]$Version,
    
    [string]$MinAllowed = "1.0.0",
    [switch]$Mandatory,
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"

$releaseDir = "out\releases\latest"
$loaderPath = "out\bin\Release\CobbleLoader.exe"
$dllPath = "out\bin\Release\Cobble.dll"
$privateKeyPath = "infra\keys\private.key"


if (!(Test-Path $loaderPath)) { throw "Loader not found: $loaderPath" }
if (!(Test-Path $dllPath)) { throw "DLL not found: $dllPath" }
if (!(Test-Path $privateKeyPath)) { throw "Private key not found. Run .\tools\generate_keys.ps1 first" }


New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null


Copy-Item $loaderPath "$releaseDir\CobbleLoader.exe" -Force
Copy-Item $dllPath "$releaseDir\Cobble.dll" -Force


$loaderHash = (Get-FileHash "$releaseDir\CobbleLoader.exe" -Algorithm SHA256).Hash.ToLower()
$dllHash = (Get-FileHash "$releaseDir\Cobble.dll" -Algorithm SHA256).Hash.ToLower()

$loaderSize = (Get-Item "$releaseDir\CobbleLoader.exe").Length
$dllSize = (Get-Item "$releaseDir\Cobble.dll").Length


$manifest = @{
    version = $Version
    min_allowed = $MinAllowed
    mandatory = $Mandatory.IsPresent
    loader = @{
        sha256 = $loaderHash
        size = $loaderSize
    }
    dll = @{
        sha256 = $dllHash
        size = $dllSize
    }
    notes = $Notes
}

$manifestJson = $manifest | ConvertTo-Json -Compress
$manifestBytes = [System.Text.Encoding]::UTF8.GetBytes($manifestJson)


[System.IO.File]::WriteAllBytes("$releaseDir\manifest.json", $manifestBytes)



$signScript = @"
import sys
import base64
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

# Read private key (raw 32 bytes)
with open('$privateKeyPath', 'rb') as f:
    key_bytes = f.read()

# If it's raw 32 bytes, load directly
if len(key_bytes) == 32:
    private_key = Ed25519PrivateKey.from_private_bytes(key_bytes)
else:
    # Try PEM format
    private_key = serialization.load_pem_private_key(key_bytes, password=None)

# Read manifest
with open('$releaseDir/manifest.json', 'rb') as f:
    manifest = f.read()

# Sign
signature = private_key.sign(manifest)

# Write signature
with open('$releaseDir/manifest.sig', 'wb') as f:
    f.write(signature)

print('Signed successfully')
"@


try {
    $signScript | python -
    Write-Host "[OK] Manifest signed with Ed25519" -ForegroundColor Green
} catch {
    Write-Host "[WARN] Python signing failed. Using Go fallback..." -ForegroundColor Yellow
    
    
    $goSign = @"
package main

import (
    "crypto/ed25519"
    "os"
)

func main() {
    keyBytes, _ := os.ReadFile("$privateKeyPath")
    manifest, _ := os.ReadFile("$releaseDir/manifest.json")
    
    privateKey := ed25519.PrivateKey(keyBytes)
    sig := ed25519.Sign(privateKey, manifest)
    
    os.WriteFile("$releaseDir/manifest.sig", sig, 0644)
}
"@
    $goSign | Set-Content "temp_sign.go"
    go run temp_sign.go
    Remove-Item "temp_sign.go"
    Write-Host "[OK] Manifest signed with Ed25519 (Go)" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Release $Version ===" -ForegroundColor Cyan
Write-Host "Loader: $loaderHash ($loaderSize bytes)"
Write-Host "DLL:    $dllHash ($dllSize bytes)"
Write-Host "Mandatory: $($Mandatory.IsPresent)"
Write-Host ""
Write-Host "Files created in: $releaseDir"
