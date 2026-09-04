Suggested implementation phases

## Phase 1 (MVP)

- GoogleDriveClient::downloadFile(id, localPath)
- ZipArchive::extract(zip, mapper) — map zip entry → absolute path 
- CloudSaveService::listBackups() / runRestore(backupId, merge=true) 
- Pre-restore ZIP on SD 
- Settings UI: backup list + confirm Hard block if not idle / no network

## Phase 2

- manifest.json in uploads
- Path mismatch warnings in summary “Undo last restore” from pre_restore ZIP (one tap)

## Phase 3

- Per-system / per-file picker
- Retention (“keep last 10 backups on Drive”)
- UX copy that sets expectations
- “Restore merges files from cloud. It does not delete saves that aren’t in this backup.”
- “A local backup of your current saves is created first.”
- “Close RetroArch and don’t play during restore.”
- “ROM-folder saves are restored next to your ROMs; central saves go to your current RetroArch folders.”

