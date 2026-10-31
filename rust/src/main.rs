mod base64;

fn get_args() -> Option<(String, String, Option<String>)> {
    let mut args = std::env::args();
    args.next()?;

    let cmd = args.next()?;
    let src_file = args.next()?;
    let dst_file = args.next();

    if args.next().is_some() || cmd != "encode" && cmd != "decode" {
        return None;
    }

    Some((cmd, src_file, dst_file))
}

fn main() {
    let (cmd, src_file, dst_file) = match get_args() {
        Some(args) => args,
        None => {
            eprintln!("Usage:");
            eprintln!("base64 encode <src_file> <dst_file>?");
            eprintln!("base64 decode <src_file> <dst_file>?");
            return;
        }
    };

    let src = match std::fs::read(&src_file) {
        Ok(src) => src,
        Err(_) => {
            eprintln!("Could not read file '{src_file}'.");
            return;
        }
    };

    let dst: Vec<u8> = match cmd.as_str() {
        "encode" => base64::encode(&src).iter().map(|digit| digit.to_u8()).collect(),
        _ => match base64::decode(&src) {
            Ok(dst) => dst,
            Err(err) => {
                eprintln!("{err}");
                return;
            }
        },
    };

    match &dst_file {
        Some(dst_file) => match std::fs::write(dst_file, dst) {
            Ok(_) => println!("Data was successfully written to file '{dst_file}'."),
            Err(err) => eprintln!("Could not write to file '{dst_file}': {err}."),
        }
        None => match String::from_utf8(dst) {
            Ok(dst) => println!("{dst}"),
            Err(err) => eprintln!("Could not print output: {err}."),
        },
    }
}
