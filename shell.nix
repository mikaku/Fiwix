with import <nixpkgs> { };

pkgsCross.i686-embedded.stdenv.mkDerivation {
  name = "env";
}

