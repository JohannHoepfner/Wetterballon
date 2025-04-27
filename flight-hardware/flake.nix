{
  inputs = {
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      esp-dev,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (system: {
      devShells = {
        default = esp-dev.devShells.${system}.esp-idf-full;
        esp-idf-full = esp-dev.devShells.${system}.esp-idf-full;
        esp32c3-idf = esp-dev.devShells.${system}.esp32c3-idf;
        esp32s3-idf = esp-dev.devShells.${system}.esp32s3-idf;
        esp32c6-idf = esp-dev.devShells.${system}.esp32c6-idf;
      };
    });
}
