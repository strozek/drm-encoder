using System;
using System.Collections;
using System.Configuration;
using System.IO;
using System.Text;
using System.Security.Cryptography;

/// <summary>
/// This class contains various static methods which are stateless and are used throughout the main
/// application.
/// </summary>
class Utility
{
	/// <summary>
	/// Reverse an array of bytes if we are on a little endian system
	/// </summary>
	/// <param name="array">The array to be reversed, in place</param>
    public static void Reverse(byte[] array)
    {
        if(BitConverter.IsLittleEndian)
        {
            Array.Reverse(array);
        }
    }
    
	/// <summary>
	/// Convert a string representation of the atom type (4 characters) to a 32-bit unsigned integer
	/// </summary>
	/// <param name="type">The string containing the atom type</param>
	/// <returns>An unsigned integer representation of the string</returns>
	public static uint TypeToUInt(string type)
	{
		byte[] tmp = Encoding.ASCII.GetBytes(type);
		return BitConverter.ToUInt32(tmp, 0);
	}

	/// <summary>
	/// Convert a 32-bit unsigned integer to a string representation of the atom type
	/// </summary>
	/// <param name="type">The unsigned integer representation of the string</param>
	/// <returns>The string containing the atom type</returns>
	public static string TypeToString(uint type)
	{
		byte[] tmp = BitConverter.GetBytes(type);
		return Encoding.ASCII.GetString(tmp);
	}

	/// <summary>
	/// Convert the entire input string to an array of bytes.  The string must be
	/// of the form hh-hh-...-hh where h is a hex digit.
	/// </summary>
	/// <param name="hexstring">The string to convert</param>
	/// <returns>An array of bytes from the string's encoding</returns>
    public static byte[] ToByteArray(string hexstring)
    {
        int size = (hexstring.Length+1)/3;
        return ToByteArray(hexstring, size);
    }
    
	/// <summary>
	/// Convert a hexadecimal string to an array of bytes, possibly padding the size
	/// to a specific number.  The string must be of the form hh-hh-...-hh where h
	/// is a hex digit.
	/// </summary>
	/// <param name="hexstring">The string to convert</param>
	/// <param name="padsize">The size of the output array.  Must be at least the number of bytes encoded in the string</param>
	/// <returns>An array of bytes from the string's encoding</returns>
	public static byte[] ToByteArray(string hexstring, int padsize)
	{
		if((hexstring.Length+1)%3 != 0)
		{
			throw new Exception("Something wrong with hex string: "+hexstring);
		}
        if(padsize<(hexstring.Length+1)/3)
        {
            throw new Exception("Padding size cannot be less than decoded length");
        }
		byte[] result = new byte[padsize];
		
		for(int i=0; i<hexstring.Length; i+=3)
		{
			result[i/3] = byte.Parse(hexstring.Substring(i, 2), System.Globalization.NumberStyles.HexNumber);
		}
		return result;
	}

	/// <summary>
	/// A wrapper around the encryption scheme.  Encrypt or decrypt an array of bytes using the key and the iv.
	/// </summary>
	/// <param name="Buf">The array of bytes to encrypt/decrypt, in place</param>
	/// <param name="Offset">Offset within Buf from which to start</param>
	/// <param name="Count">The number of bytes to convert (rounded to a multiple of 16)</param>
	/// <param name="Key">The key for encryption/decryption</param>
	/// <param name="IV">The IV for encryption/decryption</param>
	/// <param name="isEncrypt">true if the data should be encrypted, false -- if it should be decrypted</param>
	public static void RijndaelCrypt( byte [] Buf, int Offset, int Count,
		byte [] Key, byte [] IV, bool isEncrypt )
	{
		Rijndael alg = Rijndael.Create();
		alg.Mode = CipherMode.CBC;
		alg.Padding = PaddingMode.None;

		MemoryStream ms = new MemoryStream();
		
		ICryptoTransform ct;
		if(isEncrypt)
		{
			ct = alg.CreateEncryptor( Key, IV );
		}
		else
		{
			ct = alg.CreateDecryptor( Key, IV );
		}
		CryptoStream cs = new CryptoStream( ms, ct, CryptoStreamMode.Write );
		cs.Write( Buf, Offset, (Count / 16) * 16 );
		cs.Close();

		ms.ToArray().CopyTo( Buf, Offset );
	}

	/// <summary>
	/// Dump an array of data on screen, in nice visual form
	/// </summary>
	/// <param name="data">The array of data to dump on screen</param>
	static public void Dump(byte[] data)
	{
		string txt = "";
		if(data!=null)
		{
			for(int i=0; i<data.Length; ++i)
			{
				Console.Write("{0:x2} ", data[i]);
				txt	+= ((char)data[i]).ToString();
				if(i%20==19)
				{
					Console.WriteLine(" {0}", txt);
					txt = "";
				}
			}
			if(data.Length%20!=0)
			{
				Console.WriteLine(" {0}", txt);
			}
		}
	}

}

/// <summary>
/// The class containing the entire definition of the mp4 file.
/// </summary>
class Atoms
{
	private ArrayList _atoms;	// An ArrayList of other atoms which are the current atom's children
	private long _size;			// The size of the current atom (excluding the first 8 bytes)
	private long _position;		// The position of the atom in the entire collection
	private uint _type;			// The type of the atom (use TypeToString to convert to string)
	private byte[] _data;		// The atom's data

	/// <summary>
	/// Returns true if the atom specified by a particular type is a container (i.e. NECESSARILY contains 
	/// other atoms after the data chunk)
	/// </summary>
	/// <param name="type">The atom type</param>
	/// <returns>true if the atom represented by the type is a container</returns>
	public static bool IsContainer(uint type)
	{
		ArrayList containers = new ArrayList(new string[]
			{
				"moov", "trak", "udta", "edts", "mdia", "minf", "stbl", "sinf", "schi", "ilst",
				"aART", "clip", "covr", "cpil", "dinf", "disk", "drms", "matt", "mp4a", "stsd",
				"tmpo", "trkn", "SINF", "----", "\u00A9alb", "\u00A9ART", "\u00A9cmt", "\u00A9day",
				"\u00A9gen", "\u00A9grp", "\u00A9nam", "\u00A9too", "\u00A9wrt", "r00t"
			});
		string typeString = Utility.TypeToString(type);
		return containers.Contains(typeString);
	}

	/// <summary>
	/// Get or set the atom's type
	/// </summary>
	public string Type
	{
		get {return Utility.TypeToString(_type);}
		set {_type = Utility.TypeToUInt(value);}
	}

	/// <summary>
	/// Get or set the atom's data
	/// </summary>
	public byte[] Data
	{
		get {return _data;}
		set {_data = value;}
	}

	/// <summary>
	/// Get or set the atom's position within the collection
	/// </summary>
	public long Position
	{
		get {return _position;}
		set {_position = value;}
	}

	/// <summary>
	/// Create an atom.  Every atom has a binary reader associated with it, which parses the data in.
	/// This is the constructor for a container atom -- after (possibly) reading the data chunk, the
	/// data from the binary reader is parsed in search of child atoms
	/// </summary>
	/// <param name="br">The binary reader associated with the parses</param>
	/// <param name="type">Atom type</param>
	/// <param name="size">The size of the atom (excluding the first 8 bytes)</param>
	public Atoms(BinaryReader br, uint type, long size)
	{
		_atoms = new ArrayList();
		_type = type;
		_position = br.BaseStream.Position-8;
		_size = size;
		if(IsContainer(type)==false)
		{
			_data = br.ReadBytes((int)size);
			return;
		}
		switch(Utility.TypeToString(type))
		{
			case "uuid": _data = br.ReadBytes(16); break;
			case "stsd": _data = br.ReadBytes(8); break;
			case "mp4a": _data = br.ReadBytes(28); break;
			case "drms": _data = br.ReadBytes(28); break;
			default: break;
		}
		Parse(br);
	}

	/// <summary>
	/// Create an atom.  This constructor creates a non-container atom, given specific data.  This
	/// is used for adding custom atoms to the collection, not necessarily coming from a parsed source.
	/// </summary>
	/// <param name="type">The atom type</param>
	/// <param name="data">The data to be associated with the atom</param>
	public Atoms(uint type, byte[] data)
	{
		_atoms = new ArrayList();
		_type = type;
		_position = 0;
		if(data!=null)
		{
			_size = data.Length;
			_data = new byte[data.Length];
			data.CopyTo(_data, 0);
		}
		else
		{
			_size = 0;
			_data = null;
		}
	}

	/// <summary>
	/// Parse the data from the binary reader, creating a structure of atoms (the atom tree).  This method
	/// reads the entire data from the binary reader.  It currently doesn't suppose size arguments that take
	/// 8 bytes -- so really long mp4 files cannot be read
	/// </summary>
	/// <param name="br">The binary reader associated with the parser</param>
	public void Parse(BinaryReader br)
	{
		while(br.BaseStream.Position < _position+_size)
		{
			byte[] tmp;

			tmp = br.ReadBytes(4); Utility.Reverse(tmp); 
			long size = BitConverter.ToUInt32(tmp, 0);
			tmp = br.ReadBytes(4);
			uint type = BitConverter.ToUInt32(tmp, 0);

			if(size == 1)
			{
				throw new Exception("This version does not support long size arguments.");
			}

			_atoms.Add(new Atoms(br, type, size-8));
		}
	}

	/// <summary>
	/// Write out the representation of the atom tree to the filea, starting with the current atom.
	/// This is a recursive function -- each child calls it in sequence to create the tree
	/// </summary>
	/// <param name="ms">The MemoryStream to write the tree to</param>
	public void Write(MemoryStream ms)
	{
		if(_data!=null)
		{
			ms.Write(_data, 0, _data.Length);
		}
		foreach(Atoms atom in _atoms)
		{
			byte[] tmp;

			tmp = BitConverter.GetBytes((int)(atom._size+8)); Utility.Reverse(tmp);
			ms.Write(tmp, 0, 4);
			tmp = BitConverter.GetBytes(atom._type);
			ms.Write(tmp, 0, 4);
			atom.Write(ms);
		}
	}

	/// <summary>
	/// Render the tree on screen -- this method is purely for diagnostics.
	/// </summary>
	/// <param name="lastItem">An ArrayList of bools, each equal to true if the item at a particular level is the last child</param>
	/// <param name="level">The current depth of the tree</param>
	public void DrawTree(ArrayList lastItem, int level)
	{
		int whichAtom = 0;
		if(lastItem.Count<=level)
		{
			lastItem.Add(false);
		}
		else
		{
			lastItem[level] = false;
		}
		foreach(Atoms atom in _atoms)
		{
			if(whichAtom == _atoms.Count-1)
			{
				lastItem[level] = true;
			}
			Console.Write("{0:d8} ", atom.Position);

			// This is used to draw out nice branches of the tree that terminate on ultimate items
			for(int i=0; i<level; ++i)
			{
				Console.Write(" {0}   ", ((bool)lastItem[i] ? " ":"|"));
			}
			Console.Write(" +-- {0}", Utility.TypeToString(atom._type));
			if(atom._data!=null)
			{
				Console.Write(" *{0}", atom.Data.Length);
			}
			Console.WriteLine(" ({0})", atom._size+8);

			atom.DrawTree(lastItem, level+1);
			++whichAtom;
		}
	}

	/// <summary>
	/// Append an atom to the current list of children, of a specified type and with specified data
	/// </summary>
	/// <param name="typeString">The string representation of the atom type</param>
	/// <param name="data">The data to pass to the atom</param>
	/// <returns>A new atom which is the child of the current atom</returns>
	public Atoms AppendAtom(string typeString, byte[] data)
	{
		uint type = Utility.TypeToUInt(typeString);
		Atoms atom = new Atoms(type, data);
		_atoms.Add(atom);
		return atom;
	}

	/// <summary>
	/// Lookup an atom from the list of the current atom's children by name, or return null
	/// if no atom found
	/// </summary>
	/// <param name="typeString">The string representing the atom type</param>
	/// <returns>The atom if such child exists or null otherwise</returns>
	public Atoms Child(string typeString)
	{
		uint type = Utility.TypeToUInt(typeString);
		foreach(Atoms atom in _atoms)
		{
			if(atom._type == type) return atom;
		}
		return null;
	}

	/// <summary>
	/// This function updates the Position and Size fields of the entire atom tree (starting with
	/// the current atom, but leaving the Position field of the current atom unchanged) 
	/// and returns the Size of the current atom.
	/// </summary>
	/// <returns>The Size of the current atom.</returns>
	public long GetAndUpdateSize()
	{
		long size = 0;
		if(_data!=null)
		{
			size+=_data.Length;
		}
		foreach(Atoms atom in _atoms)
		{
			atom.Position = size+_position+8;
			size+=atom.GetAndUpdateSize()+8;
		}
		_size = size;
		return size;
	}

	/// <summary>
	/// Return an array of bytes representing the entire atom tree (uses the Write method on the current atom)
	/// </summary>
	public byte[] Bytes
	{
		get
		{
			MemoryStream ms = new MemoryStream();
			Write(ms);
			return ms.ToArray();
		}
	}
}

/// <summary>
/// The main class, the frontend for the application
/// </summary>
class Encrypt
{
	FileStream _input = null;	// The input stream
	FileStream _output = null;	// The output stream
	string _infile;				// The input file
	string _outfile;			// The output file
	Atoms _root;				// The root atom (its 8 bytes are not saved anywhere)
	uint[] _table;				// The sample table for the media file

	/// <summary>
	/// The constructor sets the input and the output file strings
	/// </summary>
	/// <param name="infile">The input file (the m4a file)</param>
	/// <param name="outfile">The output file (the m4p file)</param>
	public Encrypt(string infile, string outfile)
	{
		_infile = infile;
		_outfile = outfile;
	}

	/// <summary>
	/// The constructor sets the input file string
	/// </summary>
	/// <param name="infile">The input file (the m4a file)</param>
	public Encrypt(string infile)
	{
		_infile = infile;
		_outfile = null;
	}

	/// <summary>
	/// Open the input file for reading, and the output file for writing
	/// </summary>
	public void Open()
	{
		_input = new FileStream(_infile, FileMode.Open, FileAccess.Read);
		if(_outfile!=null)
		{
			_output = new FileStream(_outfile, FileMode.Create, FileAccess.Write);
		}
	}

	/// <summary>
	/// Parse the file and get the _root atom
	/// </summary>
	public void Parse()
	{
		BinaryReader br = new BinaryReader(_input);
		_root = new Atoms(br, Utility.TypeToUInt("r00t"), _input.Length);
	}

	/// <summary>
	/// Get the sample table and fill the _table private variable
	/// </summary>
	/// <param name="data">The data representing the sample table</param>
	public void GetSampleTable(byte[] data)
	{
		byte[] tmp = new byte[4];
		Array.Copy(data, 8, tmp, 0, 4); Utility.Reverse(tmp);
		uint numSamples = BitConverter.ToUInt32(tmp, 0);
		Console.WriteLine("Found {0} samples.", numSamples);
		_table = new uint[numSamples];
		for(int i=0; i<numSamples; ++i)
		{
			Array.Copy(data, 12+i*4, tmp, 0, 4); Utility.Reverse(tmp);
			_table[i] = BitConverter.ToUInt32(tmp, 0);
		}
	}

	/// <summary>
	/// Shift the offset of each data chunk by delta
	/// </summary>
	/// <param name="data">The data to offset (the first 4 bytes represent the number of chunks, the rest are the chunks)</param>
	/// <param name="delta">The offset by which to shift the data</param>
	public void UpdateChunkTable(byte[] data, int delta)
	{
		byte[] tmp = new byte[4];
		Array.Copy(data, 4, tmp, 0, 4); Utility.Reverse(tmp);
		uint numChunks = BitConverter.ToUInt32(tmp, 0);
		Console.WriteLine("Found {0} chunks, offsets shifted by {1}.", numChunks, delta);
		for(int i=0; i<numChunks; ++i)
		{
			Array.Copy(data, 8+i*4, tmp, 0, 4); Utility.Reverse(tmp);
			uint chunk = BitConverter.ToUInt32(tmp, 0);
			if(delta>0)
			{
				// This is tricky since chunk is unsigned!
				chunk += (uint)delta;
			}
			else
			{
				chunk -= (uint)(-delta);
			}
			tmp = BitConverter.GetBytes(chunk); Utility.Reverse(tmp);
			Array.Copy(tmp, 0, data, 8+i*4, 4);
		}
	}

	/// <summary>
	/// The main processor -- convert a decrypted atom tree to an encrypted one.  See in-line comments.
	/// </summary>
	/// <returns>true if the operation was successful</returns>
	public bool Process()
	{
		// Get the position of mdat before changes -- this will be necessary for chunk table offseting
		long positionBefore = _root.Child("mdat").Position;

		// Find the mp4a atom -- if not found, we're given a wrong file
		Atoms mp4a = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stsd").Child("mp4a");
		if(mp4a==null || _outfile==null)
		{
			return false;
		}
		Atoms sinf = mp4a.AppendAtom("sinf", null);

		// Append an atom to sinf, containing the data "mp4a"
		byte[] frma = Encoding.ASCII.GetBytes("mp4a");
		sinf.AppendAtom("frma", frma);

		// Append an atom to sinf, containing the data "itun" and 8 NULLs.  Then append the schi atom to sinf
		byte[] schm = new byte[12];
		schm.Initialize();
		Encoding.ASCII.GetBytes("itun", 0, 4, schm, 4);

		sinf.AppendAtom("schm", schm);
		Atoms schi = sinf.AppendAtom("schi", null);

		// Grab the private key from the configuration file
		byte[] privateKey = Utility.ToByteArray(ConfigurationSettings.AppSettings["privateKey"]);
        
		// Append the user atom to schi
		schi.AppendAtom("user", Utility.ToByteArray(ConfigurationSettings.AppSettings["user"]));

		// Grab the key from the configuration file (obtained using iTunes' server) and append to schi
		byte[] keyData = BitConverter.GetBytes(Convert.ToInt32(ConfigurationSettings.AppSettings["key"]));
		Utility.Reverse(keyData);
		schi.AppendAtom("key ", keyData);

		// Grab the iviv and append to schi
		byte[] ivivData = Utility.ToByteArray(ConfigurationSettings.AppSettings["iviv"]);
		schi.AppendAtom("iviv", ivivData);

		// Grab the righ and append to schi
		byte[] righData = Utility.ToByteArray(ConfigurationSettings.AppSettings["righ"], 80);
		schi.AppendAtom("righ", righData);

		// Append the name atom (with the user's name, followed by NULLs) to schi
		byte[] nameData = new byte[256];
		Encoding.ASCII.GetBytes(ConfigurationSettings.AppSettings["name"]).CopyTo(nameData, 0);
		string nameString = Encoding.ASCII.GetString(nameData);
		schi.AppendAtom("name", nameData);

		// Append the priv data to schi, and encrypt it
		byte[] privData = Utility.ToByteArray(ConfigurationSettings.AppSettings["priv"], 440);
		schi.AppendAtom("priv", privData);
		byte[] privDataEncrypted = new byte[440];
		privData.CopyTo(privDataEncrypted, 0);

		// Hash the name and the iviv
		MD5CryptoServiceProvider MD5 = new MD5CryptoServiceProvider();
		byte[] nameHashed = new byte[16];
		MD5.TransformBlock(nameData, 0, nameString.IndexOf('\0'), nameHashed, 0);
		MD5.TransformFinalBlock(ivivData, 0, ivivData.Length);

		// Perform the encryption of the hash to get the new key and iv
		Utility.RijndaelCrypt(privData, 0, privData.Length, privateKey, MD5.Hash, false);
		if(Encoding.ASCII.GetString(privData, 0, 4)!="itun")
		{
			throw new Exception("The priv atom, the privateKey, or the name and iviv fields are invalid");
		}
		byte[] mdatKey = new byte[16];
		Array.Copy(privData, 24, mdatKey, 0, 16);
		byte[] mdatIV = new byte[16];
		Array.Copy(privData, 48, mdatIV, 0, 16);

		// Get the sample table
		Atoms stbl = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl");
		Atoms stsz = stbl.Child("stsz");
		GetSampleTable(stsz.Data);
		byte[] mdatData = _root.Child("mdat").Data;
		int offset = 0;

		// Encrypt all the chunks using the new key and iviv
		for(int i=0; i<_table.Length; ++i)
		{
			Utility.RijndaelCrypt(mdatData, offset, (int)_table[i], mdatKey, mdatIV, true);
			offset += (int)_table[i];
		}

		// Change the type from m4pa to drms.  Update chunk sizes (since we've appended quite a lot!)
		mp4a.Type = "drms";
		_root.GetAndUpdateSize();

		// Update the chunk table (since the offsets shifted)
		long positionAfter = _root.Child("mdat").Position;
		Atoms stco = stbl.Child("stco");
		UpdateChunkTable(stco.Data, (int)(positionAfter - positionBefore));

		// Write the resulting data to the output file
		byte[] data = _root.Bytes;
		_output.Write(data, 0, data.Length);
		return true;
	}

	/// <summary>
	/// Report the private fields from an already encrypted m4p file.  The method takes a Music Store
	/// encrypted file, and reports the user, key, iviv, righ, name and priv fields, to be used in the 
	/// configuration file.  The private key is also read and converted.  The file containing the private
	/// key must exist in the current directory.  The file has the name xxxxxxxx.yyy, where x is the user
	/// and y is the key
	/// </summary>
	/// <returns>true on success</returns>
	public bool Report()
	{	
		Atoms drms = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stsd").Child("drms");
		if(drms==null)
		{
			return false;
		}
		string user = BitConverter.ToString(drms.Child("sinf").Child("schi").Child("user").Data);
		Console.WriteLine("user: {0}", user);
		byte[] tmp = new byte[4];
		drms.Child("sinf").Child("schi").Child("key ").Data.CopyTo(tmp, 0);
		Utility.Reverse(tmp);
		int key = BitConverter.ToInt32(tmp, 0);
		Console.WriteLine("key : {0:d3}", key);
		Console.WriteLine("iviv: {0}", BitConverter.ToString(drms.Child("sinf").Child("schi").Child("iviv").Data));
		byte[] righ = new byte[80];
		byte[] righTrimmed = null;
		drms.Child("sinf").Child("schi").Child("righ").Data.CopyTo(righ, 0);
		for(int i=79; i>=0; --i)
		{
			if(righ[i]!=0)
			{
				righTrimmed = new byte[i+1];
				Array.Copy(righ, righTrimmed, i+1);
				break;
			}
		}
		Console.WriteLine("righ: {0}", BitConverter.ToString(righTrimmed));
		string name = Encoding.ASCII.GetString(drms.Child("sinf").Child("schi").Child("name").Data);
		name = name.Remove(name.IndexOf('\0'), name.Length-name.IndexOf('\0'));
		Console.WriteLine("name: {0}", name);
		Console.WriteLine("priv: {0}", BitConverter.ToString(drms.Child("sinf").Child("schi").Child("priv").Data));
		string filename = string.Format("{0}.{1:d3}", user.Replace("-", ""), key);
		FileStream fs = new FileStream(filename, FileMode.Open, FileAccess.Read);
		byte[] read = new byte[16];
		fs.Read(read, 0, 16);
		Console.WriteLine("privateKey: {0}", BitConverter.ToString(read));
		fs.Close();
		return true;
	}

	/// <summary>
	/// Draw out the file tree structure
	/// </summary>
	public void DrawTree()
	{
		ArrayList lastItem = new ArrayList();
		_root.DrawTree(lastItem, 0);
	}

	/// <summary>
	/// Close the streams
	/// </summary>
	public void Close()
	{
		_input.Close();
		if(_outfile!=null)
		{
			_output.Close();
		}
	}

	public static void Main(string[] Args)
	{
		if( Args.Length < 1 || Args.Length > 2)
		{
			Console.WriteLine("Usage: m4aEncrypt.exe infile outfile    to encrypt the m4a file");
			Console.WriteLine("       m4aEncrypt.exe infile            to report the crypto keys in the m4p file");
			return;
		}
		Encrypt encrypt;
		if(Args.Length == 2)
		{
			encrypt = new Encrypt(Args[0], Args[1]);
		}
		else
		{
			encrypt = new Encrypt(Args[0]);
		}
		encrypt.Open();
		encrypt.Parse();
		encrypt.DrawTree();

		// Either report what's in the encrypted file, or process the encrypted file
		if(encrypt.Report()==false)
		{
			encrypt.Process();
			encrypt.DrawTree();
		}
		encrypt.Close();
	}
}
