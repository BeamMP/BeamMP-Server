use std::path::{Path, PathBuf};

/// Ensures the given path exists by creating it if it doesn't.
pub fn ensure_path_exists(path: &PathBuf) -> anyhow::Result<()> {
    if !path.exists() {
        debug!("Path {:?} doesn't exist, creating it", path);
        std::fs::create_dir_all(path)?;
    }
    Ok(())
}

/// Joins a parent folder and a sub-path, resolving the subpath beforehand to ensure that
/// the resulting path is still within the parent folder, regardless of ".." in the sub-path.
pub fn join_path_secure(parent: &Path, sub: &Path) -> anyhow::Result<PathBuf> {
    Ok(parent.join(sub.canonicalize()?.as_path()))
}

/// Converts a PathBuf into a String in a lossy way. This is generally the way we want to do it
/// in the server.
pub fn path_to_string(path: PathBuf) -> String {
    path.into_os_string().to_string_lossy().to_string()
}
