{
  name = "my-project";
  version = "1.0";

  buildInputs = [
    pkgs.hello
  ];

  buildPhase = { stdenv }: ''
    echo "building"
  '';

  inherit (pkgs) gcc cmake;
}
