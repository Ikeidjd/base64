use std::fmt::Display;

#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct u6(u8);

impl u6 {
    pub const fn new(value: u8) -> Self {
        Self(value & 0b111111)
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub struct Base64Digit(u8);

impl Base64Digit {
    pub const PADDING: Self = Self(b'=');

    pub const fn from_value(value: u6) -> Self {
        let value = value.0;

        Self(match value {
            ..26 => value + b'A',
            ..52 => value - 26 + b'a',
            ..62 => value - 52 + b'0',
            62 => b'+',
            63 => b'/',
            _ => unreachable!(),
        })
    }

    pub const fn from_u8(c: u8) -> Option<Self> {
        match c {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'+' | b'/' => Some(Self(c as u8)),
            _ => None,
        }
    }

    pub const fn to_value(&self) -> u6 {
        let digit = self.0;

        u6::new(match digit {
            b'A'..=b'Z' => digit - b'A',
            b'a'..=b'z' => digit - b'a' + 26,
            b'0'..=b'9' => digit - b'0' + 52,
            b'+' => 62,
            b'/' => 63,
            _ => unreachable!(),
        })
    }

    pub const fn to_u8(&self) -> u8 {
        self.0
    }
}

fn triplet_to_quartet(triplet: [u8; 3]) -> [u6; 4] {
    [
        u6::new(triplet[0] >> 2),
        u6::new((triplet[0] << 4) | (triplet[1] >> 4)),
        u6::new((triplet[1] << 2) | (triplet[2] >> 6)),
        u6::new(triplet[2]),
    ]
}

pub fn encode(bytes: &[u8]) -> Vec<Base64Digit> {
    let mut bytes: Vec<u8> = bytes.iter().cloned().collect();
    let mut padding = 0;

    while bytes.len() % 3 != 0 {
        bytes.push(0);
        padding += 1;
    }

    let mut base64 = Vec::new();

    for i in (0..bytes.len()).step_by(3) {
        let mut triplet = [0; 3];

        for (j, &byte) in bytes[i..i + 3].iter().enumerate() {
            triplet[j] = byte;
        }

        let quartet = triplet_to_quartet(triplet);
        base64.extend(quartet.iter().map(|&value| Base64Digit::from_value(value)));
    }

    for i in base64.len() - padding..base64.len() {
        base64[i] = Base64Digit::PADDING;
    }

    base64
}

pub enum DecodeError {
    NonMultipleOf4Length,
    InvalidPadding(usize),
    InvalidDigit(usize, u8),
}

impl Display for DecodeError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "Invalid base64 string: ")?;

        match *self {
            DecodeError::NonMultipleOf4Length => write!(f, "length must be a multiple of 4. This might be fixed by adding one or two '=' characters at the end"),
            DecodeError::InvalidPadding(i) => write!(f,
                "padding character {} is only allowed at the end, a maximum of two times in a row, but was found at position {i} (starting from 0))", Base64Digit::PADDING.0 as char),
            DecodeError::InvalidDigit(i, byte) => write!(f, "invalid base64 digit '{}' at position {i} (starting from 0)", byte as char),
        }?;

        write!(f, ".")
    }
}

fn quartet_to_triplet(quartet: [u6; 4]) -> [u8; 3] {
    [
        (quartet[0].0 << 2) | (quartet[1].0 >> 4),
        (quartet[1].0 << 4) | (quartet[2].0 >> 2),
        (quartet[2].0 << 6) | (quartet[3].0 >> 0),
    ]
}

fn decode_digits_mut(base64: &mut [Base64Digit]) -> Result<Vec<u8>, DecodeError> {
    if base64.len() % 4 != 0 {
        return Err(DecodeError::NonMultipleOf4Length);
    }

    let mut padding = 0;

    for i in (0..base64.len()).rev() {
        if base64[i] == Base64Digit::PADDING {
            padding += 1;
            base64[i] = Base64Digit::from_value(u6::new(0));

            if padding >= 2 {
                break;
            }
        }
    }

    let mut values = Vec::new();

    for (i, &digit) in base64.iter().enumerate() {
        if digit == Base64Digit::PADDING {
            return Err(DecodeError::InvalidPadding(i));
        }

        values.push(digit.to_value());
    }

    let mut bytes = Vec::new();

    for i in (0..values.len()).step_by(4) {
        let mut quartet = [u6::new(0); 4];

        for (j, &value) in values[i..i + 4].iter().enumerate() {
            quartet[j] = value;
        }

        let triplet = quartet_to_triplet(quartet);
        bytes.extend(triplet);
    }

    for _ in 0..padding {
        bytes.pop();
    }

    Ok(bytes)
}

pub fn decode(base64: &[u8]) -> Result<Vec<u8>, DecodeError> {
    let mut digits = Vec::new();

    for (i, &byte) in base64.iter().enumerate() {
        if byte == Base64Digit::PADDING.0 {
            digits.push(Base64Digit::PADDING);
            continue;
        }

        digits.push(match Base64Digit::from_u8(byte) {
            Some(digit) => digit,
            None => return Err(DecodeError::InvalidDigit(i, byte)),
        });
    }

    decode_digits_mut(&mut digits)
}
