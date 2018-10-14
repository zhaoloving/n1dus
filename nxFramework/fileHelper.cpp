#include "fileHelper.h"
#include <dirent.h>
#include <sys/stat.h>
#include <vector>

namespace NXFramework
{
typedef int(*qsort_compar)(const void *, const void *);

int cmpstringp(const dirent** dent1, const dirent** dent2)
{
	char isDir[2];
	char path1[512];
	char path2[512];

	// push '..' to beginning
	if (strcmp("..", (*dent1)->d_name) == 0)
		return -1;
	else
    if (strcmp("..", (*dent2)->d_name) == 0)
		return 1;

	isDir[0] = (*dent1)->d_type == DT_DIR?1:0;
	isDir[1] = (*dent2)->d_type == DT_DIR?1:0;

	u64 sizeA = GetFileSize(path1);
	u64 sizeB = GetFileSize(path2);

	u32 config_sort_by = 0;
	switch(config_sort_by)
	{
		case 0: // Alphabetical ascending
        if (isDir[0] == isDir[1]) 	return strcasecmp((*dent1)->d_name, (*dent2)->d_name);
        else        				return isDir[1] - isDir[0];
        break;

		case 1: // Alphabetical descending
        if (isDir[0] == isDir[1])	return strcasecmp((*dent2)->d_name, (*dent1)->d_name);
        else			            return isDir[1] - isDir[0];
		break;

		case 2: // Size newest
        if (isDir[0] == isDir[1])	return sizeA > sizeB ? -1 : sizeA < sizeB ? 1 : 0;
        else        				return isDir[1] - isDir[0];
        break;

		case 3: // Size oldest
        if (isDir[0] == isDir[1])  	return sizeB > sizeA ? -1 : sizeB < sizeA ? 1 : 0;
        else        				return isDir[1] - isDir[0]; // put directories first
        break;
	}
	return 0;
}

u64 GetFileSize(const char* filename)
{
	struct stat st;
	stat(filename, &st);
	return st.st_size;
}

void GetFileModifiedTime(const char* filename, char* dateString)
{
	struct stat attr;
	stat(filename, &attr);
	struct tm* timeinfo = localtime (&attr.st_mtime);
	strftime(dateString,256,"%F %I:%M %p.",timeinfo);
}

void GetSizeString(char *string, u64 size)
{
	double double_size = (double)size;
	int i = 0;
	static const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"};
	while (double_size >= 1024.0f)
	{
		double_size /= 1024.0f;
		i++;
	}
	sprintf(string, "%.*f %s", (i == 0) ? 0 : 2, double_size, units[i]);
}

const char* GetFileExt(const char *filename)
{
	const char *dot = strrchr(filename, '.');

	if (!dot || dot == filename)
		return "";

	return dot + 1;
}

void GetFileBasename (char *string, const char *filename)
{
	const char *dot = strrchr(filename, '.');

	int length = (!dot || dot == filename)?
                    (int)strlen(filename):
                    dot-filename;

	strncpy(string, filename, length);
	string[length] = '\0';
}

// Basically scndir implementation
int ScanDir(const char *dir,
            std::vector<dirent *>& namelist,
            int (*compar)(const dirent **, const dirent **))
{
	DIR *d;
	dirent *entry;
	size_t entrysize;

	if ((d=opendir(dir)) == NULL)
		return(-1);

	namelist.clear();
	while ((entry=readdir(d)) != NULL)
	{
        entrysize = sizeof(dirent) - sizeof(entry->d_name) + strlen(entry->d_name) + 1;
        namelist.push_back((dirent *)malloc(entrysize));
        memcpy(namelist.back(), entry, entrysize);
	}
	if (closedir(d))
		return(-1);

	if (compar != NULL)
		qsort((void *)(namelist.data()), namelist.size(), sizeof(dirent*), (qsort_compar)compar);

	return namelist.size();
}

void PopulateFiles(const char* dir,
                   std::vector<DirEntry>& dirEntries,
                   std::vector<std::string>& extFilter)
{
	char path[512];
    dirEntries.clear();
    LOG("PopulateFiles, path: %s\n", dir);

	// Open Working Directory
	DIR *directory = opendir(dir);
	if(directory)
	{
		// Add fake ".." entry except on root
		if(strcmp(dir, ROOT_PATH) != 0)
		{
		    DirEntry item;
			memset(&item, 0, sizeof(DirEntry));
			strcpy((char*)item.name, "..");
			item.isDir = 1;
			dirEntries.push_back(item);
		}

		std::vector<dirent *> entries;
		ScanDir(dir, entries, cmpstringp);
		if(entries.size() >= 0)
		{
			for (u32 i = 0; i < entries.size(); ++i)
			{
				// Ignore null filename
				if (entries[i]->d_name[0] == '\0')
					continue;

				// Ignore "." in all Directories
				if (strcmp(entries[i]->d_name, ".") == 0)
					continue;

				// Ignore ".." in Root Directory
				if (strcmp(dir, ROOT_PATH) == 0 && strncmp(entries[i]->d_name, "..", 2) == 0) // Ignore ".." in Root Directory
					continue;

                // Check ext filters for regular files
                if(extFilter.size() > 0 && entries[i]->d_type == DT_REG)
                {
                    bool extFound = false;
                    for(u32 j = 0 ; j < extFilter.size() ; ++j)
                    {
                        std::string fname = entries[i]->d_name;
                        if(fname.find(extFilter[j], (fname.length() - extFilter[j].length())) != std::string::npos)
                        {
                            extFound = true;
                            break;
                        }
                    }
                    if(!extFound)
                        continue;
                }
				DirEntry item;
				memset(&item, 0, sizeof(DirEntry));
				strcpy((char*)item.name, entries[i]->d_name);
				item.isDir = (entries[i]->d_type == DT_DIR);

				// Copy file size
				if(!item.isDir)
				{
					strcpy(path, dir);
					strcpy(path + strlen(path), (char*)item.name);
					item.size = GetFileSize(path);
				}
                dirEntries.push_back(item);
                LOG("New File Entry: %s\n",(char*)item.name);
				free(entries[i]);
			}
		}
        entries.clear();
		closedir(directory);
	}
}

// Navigate to Folder
int Navigate(char* cwd, DirEntry& entry, bool up)
{
    LOG("Navigate sourceDir: %s\n", cwd);
    LOG("Navigate fileEntry: %s\n", entry.name);

	// Special case ".."
	if ((up) || (strncmp((char*)entry.name, "..", 2) == 0))
	{
		char *slash = NULL;

		// Find last '/' in working directory
		int i = strlen(cwd) - 2;
		for(; i >= 0; i--)
		{
			// Slash discovered
			if (cwd[i] == '/')
			{
				slash = cwd + i + 1;    // Save pointer
				break;                  // Stop search
			}
		}
		slash[0] = 0; // Terminate working directory
	}
	// Normal folder
	else
	{
		if (entry.isDir)
		{
			// Append folder to working directory
			strcpy(cwd + strlen(cwd), (char*)entry.name);
			cwd[strlen(cwd) + 1] = 0;
			cwd[strlen(cwd)] = '/';
		}
	}
    LOG("Navigate newDir: %s\n", cwd);
	return 0; // Return success
}

int RmDirRecursive (const char* dir)
{
    std::vector<DirEntry> dirEntries;
    std::vector<std::string>extFilters;
    PopulateFiles(dir, dirEntries, extFilters);
    for(int i = 0 ; i < (int)dirEntries.size() ; ++i)
    {
        DirEntry& entry = dirEntries[i];

        // Ignore ".." in Root Directory
        if (strncmp((char*)entry.name, "..", 2) == 0) // Ignore ".."
            continue;

        std::string entryPath(std::string(dir) + "/" + std::string((char*)entry.name));
        if(entry.isDir)
        {
            LOG("Deleting folder %s...\n", entryPath.c_str());
            RmDirRecursive(entryPath.c_str());
        }
        else
        {
            LOG("Deleting file %s...\n", entryPath.c_str());
            if(remove(entryPath.c_str()) != 0)
                printf("Error deleting file\n");
        }
    }
    LOG("Deleting folder %s...\n", dir);
    if(rmdir(dir) != 0)
        printf("Error deleting folder\n");
    return 0;
}

u64 GetDirSizeNonRecursive (const char* dir)
{
    std::vector<DirEntry> dirEntries;
    std::vector<std::string>extFilters;
    PopulateFiles(dir, dirEntries, extFilters);
    u64 dirSize = 0;
    for(int i = 0 ; i < (int)dirEntries.size() ; ++i)
    {
        DirEntry& entry = dirEntries[i];

        // Ignore ".." in Root Directory
        if (strncmp((char*)entry.name, "..", 2) == 0) // Ignore ".."
            continue;

        if(!entry.isDir)
        {
            std::string entryPath(std::string(dir) + "/" + std::string((char*)entry.name));
            u64 fileSize = GetFileSize(entryPath.c_str());
            dirSize += fileSize;
            LOG("File size %s %" PRIu64 "...\n", entry.name, fileSize);
        }
    }
    LOG("Dir size %s %" PRIu64 "...\n", dir, dirSize);
    return dirSize;
}

bool FileExist(const char* filename)
{
  struct stat buffer;
  return (stat(filename, &buffer) == 0);
}

u64 GetFreeSpace(FsStorageId storage_id)
{
    u64 freeSpace;
    nsGetFreeSpaceSize(storage_id, &freeSpace);
    return freeSpace;
}

u64 GetTotalSpace(FsStorageId storage_id)
{
    u64 freeSpace;
    nsGetTotalSpaceSize(storage_id, &freeSpace);
    return freeSpace;

}

}
