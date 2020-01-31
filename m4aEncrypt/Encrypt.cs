using System;
using System.Collections;
using System.Configuration;
using System.IO;
using System.Text;
using System.Security.Cryptography;

class Utility
{
    public static void Reverse(byte[] array)
    {
        if(BitConverter.IsLittleEndian)
        {
            Array.Reverse(array);
        }
    }
    
	public static uint TypeToUInt(string type)
	{
		byte[] tmp = Encoding.ASCII.GetBytes(type);
		return BitConverter.ToUInt32(tmp, 0);
	}

	public static string TypeToString(uint type)
	{
		byte[] tmp = BitConverter.GetBytes(type);
		return Encoding.ASCII.GetString(tmp);
	}

    public static byte[] ToByteArray(string hexstring)
    {
        int size = (hexstring.Length+1)/3;
        return ToByteArray(hexstring, size);
    }
    
	public static byte[] ToByteArray(string hexstring, int padsize)
	{
		// the string must be of the form hh-hh-...-hh where h is a hex digit
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
}

class Atoms
{
	private ArrayList _atoms;
	private long _size;
	private long _position;
	private uint _type;
	private byte[] _data;

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

	public string Type
	{
		get {return Utility.TypeToString(_type);}
		set {_type = Utility.TypeToUInt(value);}
	}

	public byte[] Data
	{
		get {return _data;}
		set {_data = value;}
	}

	public long Position
	{
		get {return _position;}
		set {_position = value;}
	}

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

	public Atoms AppendAtom(string typeString, byte[] data)
	{
		uint type = Utility.TypeToUInt(typeString);
		Atoms atom = new Atoms(type, data);
		_atoms.Add(atom);
		return atom;
	}

	public Atoms Child(string typeString)
	{
		uint type = Utility.TypeToUInt(typeString);
		foreach(Atoms atom in _atoms)
		{
			if(atom._type == type) return atom;
		}
		return null;
	}

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

	public void Dump()
	{
		string txt = "";
		if(_data!=null)
		{
		    //Console.WriteLine(BitConverter.ToString(_data));
            //Console.WriteLine(Encoding.ASCII.GetString(_data));
			for(int i=0; i<_data.Length; ++i)
			{
				Console.Write("{0:x2} ", _data[i]);
				txt	+= ((char)_data[i]).ToString();
				if(i%20==19)
				{
					Console.WriteLine(" {0}", txt);
					txt = "";
				}
			}
			if(_data.Length%20!=0)
			{
				Console.WriteLine(" {0}", txt);
			}
		}
	}

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


class Encrypt
{
	FileStream _input = null;
	FileStream _output = null;
	string _infile;
	string _outfile;
	Atoms _root;
	uint[] _table;

	public Encrypt(string infile, string outfile)
	{
		_infile = infile;
		_outfile = outfile;
	}

	public void Open()
	{
		_input = new FileStream(_infile, FileMode.Open, FileAccess.Read);
		_output = new FileStream(_outfile, FileMode.Create, FileAccess.Write);
	}

	public void Parse()
	{
		BinaryReader br = new BinaryReader(_input);
		_root = new Atoms(br, Utility.TypeToUInt("r00t"), _input.Length);
	}

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

	public bool Process()
	{
		// Get the position of mdat before changes
		long positionBefore = _root.Child("mdat").Position;

		Atoms mp4a = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stsd").Child("mp4a");
		if(mp4a==null)
		{
			return false;
		}
		Atoms sinf = mp4a.AppendAtom("sinf", null);

		byte[] frma = Encoding.ASCII.GetBytes("mp4a");
		sinf.AppendAtom("frma", frma);

		byte[] schm = new byte[12];
		schm.Initialize();
		Encoding.ASCII.GetBytes("itun", 0, 4, schm, 4);

		sinf.AppendAtom("schm", schm);
		Atoms schi = sinf.AppendAtom("schi", null);

		byte[] privateKey = Utility.ToByteArray(ConfigurationSettings.AppSettings["privateKey"]);
        
		schi.AppendAtom("user", Utility.ToByteArray(ConfigurationSettings.AppSettings["user"]));

		byte[] keyData = BitConverter.GetBytes(Convert.ToInt32(ConfigurationSettings.AppSettings["key"]));
		Utility.Reverse(keyData);
		schi.AppendAtom("key ", keyData);

		byte[] ivivData = Utility.ToByteArray(ConfigurationSettings.AppSettings["iviv"]);
		schi.AppendAtom("iviv", ivivData);

		byte[] righData = Utility.ToByteArray(ConfigurationSettings.AppSettings["righ"], 80);
		schi.AppendAtom("righ", righData);

		byte[] nameData = new byte[256];
		Encoding.ASCII.GetBytes(ConfigurationSettings.AppSettings["name"]).CopyTo(nameData, 0);
		string nameString = Encoding.ASCII.GetString(nameData);
		schi.AppendAtom("name", nameData);

		byte[] privData = Utility.ToByteArray(ConfigurationSettings.AppSettings["priv"], 440);
		schi.AppendAtom("priv", privData);
		byte[] privDataEncrypted = new byte[440];
		privData.CopyTo(privDataEncrypted, 0);

		MD5CryptoServiceProvider MD5 = new MD5CryptoServiceProvider();
		byte[] nameHashed = new byte[16];
		MD5.TransformBlock(nameData, 0, nameString.IndexOf('\0'), nameHashed, 0);
		MD5.TransformFinalBlock(ivivData, 0, ivivData.Length);

		Utility.RijndaelCrypt(privData, 0, privData.Length, privateKey, MD5.Hash, false);
		if(Encoding.ASCII.GetString(privData, 0, 4)!="itun")
		{
			throw new Exception("The priv atom, the privateKey, or the name and iviv fields are invalid");
		}
		byte[] mdatKey = new byte[16];
		Array.Copy(privData, 24, mdatKey, 0, 16);
		byte[] mdatIV = new byte[16];
		Array.Copy(privData, 48, mdatIV, 0, 16);

		Atoms stbl = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl");
		Atoms stsz = stbl.Child("stsz");
		GetSampleTable(stsz.Data);
		byte[] mdatData = _root.Child("mdat").Data;
		int offset = 0;
		for(int i=0; i<_table.Length; ++i)
		{
			Utility.RijndaelCrypt(mdatData, offset, (int)_table[i], mdatKey, mdatIV, true);
			offset += (int)_table[i];
		}

		mp4a.Type = "drms";
		_root.GetAndUpdateSize();

		long positionAfter = _root.Child("mdat").Position;
		Atoms stco = stbl.Child("stco");
		UpdateChunkTable(stco.Data, (int)(positionAfter - positionBefore));

		byte[] data = _root.Bytes;
		_output.Write(data, 0, data.Length);
		return true;
	}

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

	public void DrawTree()
	{
		ArrayList lastItem = new ArrayList();
		_root.DrawTree(lastItem, 0);
	}

	/*
	public void CD(byte[] stcoData)
	{	
		byte[] tmp = new byte[4];
		for(int i=0; i<stcoData.Length; i+=4)
		{
			Array.Copy(stcoData, i, tmp, 0, 4);
			Utility.Reverse(tmp);
			Console.Write("{0:d11} ", BitConverter.ToUInt32(tmp, 0));
		}
	}

	public void CC(Encrypt from)
	{
		// from is the working file, this is the broken file
		byte[] tmp = new byte[from._root.Child("moov").Child("udta").Child("meta").Data.Length];
		_root.Child("moov").Child("udta").Child("meta").Data = tmp;

		byte[] stcoData = from._root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stco").Data;
		CD(stcoData);
		Console.WriteLine();
		byte[] stcoData2 = _root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stco").Data;
		CD(stcoData2);

		_root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stco").Data = 
		from._root.Child("moov").Child("trak").Child("mdia").Child("minf").Child("stbl").Child("stco").Data;

		_root.GetAndUpdateSize();

		byte[] data = _root.Bytes;
		_output.Write(data, 0, data.Length);
	}
	*/

	public void Close()
	{
		_input.Close();
		_output.Close();
	}

	public static void Main(string[] Args)
	{
		if( Args.Length != 2)
		{
			Console.WriteLine("Usage: m4aEncrypt.exe infile.m4a outfile.m4p");
			return;
		}
		Encrypt encrypt = new Encrypt(Args[0], Args[1]);
		encrypt.Open();
		encrypt.Parse();
		encrypt.DrawTree();
		encrypt.Report();
		encrypt.Process();
		encrypt.DrawTree();
		encrypt.Close();
	}
}
