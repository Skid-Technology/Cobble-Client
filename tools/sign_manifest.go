









package main

import (
	"crypto/ed25519"
	"flag"
	"fmt"
	"os"
)

func main() {
	keyPath      := flag.String("key",      "", "path to 32-byte Ed25519 seed file (required)")
	manifestPath := flag.String("manifest", "", "path to manifest.json to sign (required)")
	outPath      := flag.String("out",      "", "path to write the signature file (required)")
	flag.Parse()

	if *keyPath == "" || *manifestPath == "" || *outPath == "" {
		flag.Usage()
		os.Exit(1)
	}

	seed, err := os.ReadFile(*keyPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "read key: %v\n", err)
		os.Exit(1)
	}

	var priv ed25519.PrivateKey
	switch len(seed) {
	case ed25519.SeedSize: 
		priv = ed25519.NewKeyFromSeed(seed)
	case ed25519.PrivateKeySize: 
		priv = ed25519.PrivateKey(seed)
	default:
		fmt.Fprintf(os.Stderr, "unexpected key length %d (want 32 or 64)\n", len(seed))
		os.Exit(1)
	}

	manifest, err := os.ReadFile(*manifestPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "read manifest: %v\n", err)
		os.Exit(1)
	}

	sig := ed25519.Sign(priv, manifest)

	if err := os.WriteFile(*outPath, sig, 0644); err != nil {
		fmt.Fprintf(os.Stderr, "write sig: %v\n", err)
		os.Exit(1)
	}

	fmt.Printf("signed: %s -> %s\n", *manifestPath, *outPath)
}
