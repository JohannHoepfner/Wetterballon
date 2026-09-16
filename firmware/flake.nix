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
      };
    });
}
