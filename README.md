# drm-encoder
An old (2005) project to encrypt media files with Apple's FairPlay DRM

# m4aEncrypt user guide and documentation

m4aEncrypt is an application capable of encryptying AAC files.  AAC files are a type of mp4 files, which are native to iTunes.  They have a common extension .m4a.  The encrypted files (with a standard extension .m4p) are compatible with the files that can be purchased from iTunes Music Store -- they can only be played by authorized computers (up to 5 computers can be authorized at a time for a given account), and, once they are authorized, they can be uploaded onto an unlimited number of iPods.

Those properties make m4p files a perfect solution for this project: we need a file which can be uploaded to an iPod, but which, after re-downloading, cannot be played on any other computer than the original computer which encoded them.

The structure of AAC files resembles a tree structure:  an AAC file contains of a number of atoms.  Each atom contains the following data:
*	Its type (4 bytes)
*	Its size (4 bytes)
*	(optionally) The data it contains
*	(optionally) Child atoms

Atoms which have child atoms are called containers.  Only specific atoms are containers.

Below is a tree view of a typical m4a file, and the same file converted to an m4p format (the view was created by m4aEncrypt):


00000000  +-- ftyp *20 (28)
00000028  +-- moov (103323)
00000036  |    +-- mvhd *100 (108)
00000144  |    +-- iods *16 (24)
00000168  |    +-- trak (42372)
00000176  |    |    +-- tkhd *84 (92)
00000268  |    |    +-- mdia (42272)
00000276  |    |         +-- mdhd *24 (32)
00000308  |    |         +-- hdlr *29 (37)
00000345  |    |         +-- minf (42195)
00000353  |    |              +-- smhd *8 (16)
00000369  |    |              +-- dinf (36)
00000377  |    |              |    +-- dref *20 (28)
00000405  |    |              +-- stbl (42135)
00000413  |    |                   +-- stts *16 (24)
00000437  |    |                   +-- stsd *8 (103)
00000453  |    |                   |    +-- mp4a *28 (87)
00000489  |    |                   |         +-- esds *43 (51)
00000540  |    |                   +-- stsz *40028 (40036)
00040576  |    |                   +-- stsc *32 (40)
00040616  |    |                   +-- stco *1916 (1924)
00042540  |    +-- udta (60811)
00042548  |         +-- meta *60795 (60803)
00103351  +-- mdat *3716446 (3716454)


00000000  +-- ftyp *20 (28)
00000028  +-- moov (104219)
00000036  |    +-- mvhd *100 (108)
00000144  |    +-- iods *16 (24)
00000168  |    +-- trak (43268)
00000176  |    |    +-- tkhd *84 (92)
00000268  |    |    +-- mdia (43168)
00000276  |    |         +-- mdhd *24 (32)
00000308  |    |         +-- hdlr *29 (37)
00000345  |    |         +-- minf (43091)
00000353  |    |              +-- smhd *8 (16)
00000369  |    |              +-- dinf (36)
00000377  |    |              |    +-- dref *20 (28)
00000405  |    |              +-- stbl (43031)
00000413  |    |                   +-- stts *16 (24)
00000437  |    |                   +-- stsd *8 (999)
00000453  |    |                   |    +-- drms *28 (983)
00000489  |    |                   |         +-- esds *43 (51)
00000540  |    |                   |         +-- sinf (896)
00000548  |    |                   |              +-- frma *4 (12)
00000560  |    |                   |              +-- schm *12 (20)
00000580  |    |                   |              +-- schi (856)
00000588  |    |                   |                   +-- user *4 (12)
00000600  |    |                   |                   +-- key  *4 (12)
00000612  |    |                   |                   +-- iviv *16 (24)
00000636  |    |                   |                   > +-- righ *80 (88)
00000724  |    |                   |                   +-- name *256 (264)
00000988  |    |                   |                   +-- priv *440 (448)
00001436  |    |                   +-- stsz *40028 (40036)
00041472  |    |                   +-- stsc *32 (40)
00041512  |    |                   +-- stco *1916 (1924)
00043436  |    +-- udta (60811)
00043444  |         +-- meta *60795 (60803)
00104247  +-- mdat *3716446 (3716454)


The stars next to the atom type denote the size of the attached data chunk, and the numbers in parenteses denote the size of the atom (note that the size of the atom includes all the child atoms).  Notice that the difference between an unencrypted file and an encrypted file are the extra structures, denoted in red above (moreover, the mp4a atom is renamed to drms, and the data in the mdat atom is, obviously, encrypted).

The user, key, iviv, righ, name and priv fields can be obtained from a regular iTunes Music Store file.  The data is then encrypted using a three-level scheme, documented well in the code.

The program also expects the user private key, obtained from iTunes servers.  An application called Hymn (also attached in the packet) has the ability to download the private key and store it in C:\Documents and Settings\username\Application Data\drms. (to download the key, run Hymn and select FairKeys>Download your keys...)  The key should be copied to the application's current working directory.  The key filename should match the user and key fields reported by m4aEncrypt.

The program uses a configuration file which contains the data for each of the keys mentioned above.  To change the data, run m4aEncrypt with only one parameter: the name of an actual iTunes Music Store file purchased using a specific account.  The program will output the account-specific data that can then be pasted into the configuration file.  The sample configuration file is shown below:

```xml
<?xml version="1.0" encoding="utf-8" ?>
<configuration>
	<appSettings>
		<add key="user"	value="03-2C-90-73" />
		<add key="key"	value="002" />
		<add key="iviv"	value="D2-0E-CE-9D-08-33-89-15-1E-AA-3C-...-EC-B5" />
		<add key="righ"	value="76-65-49-44-00-00-00-04-70-6C-61-74-00-00-00-00-61-76-65-72-...-E6" />
		<add key="name"	value="USER" />
		<add key="priv"	value="67-C0-AB-57-67-37-1D-BC-5A-5F-9D-16-3A-7E-AB-75-38-FB-83-BC-21-D3-EE-...-92-7E" />
		<add key="privateKey"	value="BF-DB-4F-1D-48-B9-74-00-30-...-5C-06" />
	</appSettings>
</configuration>
```

After the configuration file is updated with desired account information, invoking the application with two arguments -- the input filename (the m4a file) and the output filename (the m4p file) will convert the file to an encrypted format.
