using UnrealBuildTool;
using System.IO;
using System.Linq;

public class ArcadeCabVRSDK : ModuleRules
{
    public ArcadeCabVRSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "HeadMountedDisplay",
            "SteamVR",
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string OpenVRRoot = Path.Combine(EngineDirectory, "Source", "ThirdParty", "OpenVR");

            // Prefer the newest versioned folder; skip the stub (OpenVRv000)
            string HeadersPath = null;

            if (Directory.Exists(OpenVRRoot))
            {
                var versioned = Directory.GetDirectories(OpenVRRoot)
                    .Where(d => !Path.GetFileName(d).Equals("OpenVRv000"))
                    .OrderByDescending(d => d)
                    .FirstOrDefault();

                if (versioned != null)
                    HeadersPath = Path.Combine(versioned, "headers");
            }

            if (HeadersPath != null && Directory.Exists(HeadersPath))
            {
                PrivateIncludePaths.Add(HeadersPath);

                string LibPath = Path.Combine(Path.GetDirectoryName(HeadersPath), "lib", "win64", "openvr_api.lib");
                if (File.Exists(LibPath))
                    PublicAdditionalLibraries.Add(LibPath);
            }
        }
    }
}
